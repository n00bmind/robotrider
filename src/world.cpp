/*
The MIT License

Copyright (c) 2017 Oscar Peñas Pariente <oscarpp80@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#if NON_UNITY_BUILD
#include "common.h"
#include "math_types.h"
#include "world.h"
#include "asset_loaders.h"
#include "meshgen.h"
#include "game.h"
#include "robotrider.h"
#endif


inline u32
ClusterHash( const v3i& clusterP, u32 tableSize )
{
    // TODO Better hash function! x)
    u32 hashValue = (u32)(19*clusterP.x + 7*clusterP.y + 3*clusterP.z);
    return hashValue;
}

inline u32
EntityHash( const u32& entityId, u32 tableSize )
{
    // TODO Better hash function! x)
    return entityId;
}

internal u32
CalcSimClusterIndex( v3i const& clusterRelativeP )
{
    u32 result = (clusterRelativeP.z + SimExteriorHalfSize) * SimRegionSizePerAxis * SimRegionSizePerAxis
        + (clusterRelativeP.y + SimExteriorHalfSize) * SimRegionSizePerAxis
        + (clusterRelativeP.x + SimExteriorHalfSize)
        + 1;

    return result;
}

#define INITIAL_CLUSTER_COORDS V3i( I32MAX, I32MAX, I32MAX )

void
InitWorld( World* world, MemoryArena* worldArena, MemoryArena* transientArena )
{
    RandomSeed();

    TemporaryMemory tmpMemory = BeginTemporaryMemory( transientArena );

    world->player = PUSH_STRUCT( worldArena, Player );
    world->player->mesh =
        LoadOBJ( "feisar/feisar_ship.obj", worldArena, tmpMemory,
                 M4ZRotation( PI ) * M4XRotation( PI/2 ) * M4Scale( V3( 0.02f, 0.02f, 0.02f ) ) );
    world->player->mesh.mTransform = M4Identity;

    Texture textureResult = LoadTexture( "feisar/maps/diffuse.bmp", true, true, 4 );
    Material* playerMaterial = PUSH_STRUCT( worldArena, Material );
    playerMaterial->diffuseMap = textureResult.handle;
    world->player->mesh.material = playerMaterial;

    new (&world->clusterTable) HashTable<v3i, Cluster, ClusterHash>( worldArena, 256*1024 );
    new (&world->liveEntities) BucketArray<LiveEntity>( worldArena, 256 );
    new (&world->entityRefs) HashTable<u32, StoredEntity *, EntityHash>( worldArena, 1024 );

    world->originClusterP = V3iZero;
    world->lastOriginClusterP = INITIAL_CLUSTER_COORDS;

#if 0
    MeshGeneratorRoomData* roomData = PUSH_STRUCT( worldArena, MeshGeneratorRoomData );
    roomData->areaSideMeters = world->marchingAreaSize;
    roomData->resolutionMeters = world->marchingCubeSize;
    MeshGenerator roomGenerator = { MeshGeneratorRoomFunc, &roomData->header };
    // TODO Check if we need to reinstate these pointers after a reload
    world->meshGenerators[GenRoom] = MeshGeneratorRoomFunc;
#endif

    const u32 coreThreadsCount = 1; //globalPlatform.coreThreadsCount;
    world->samplingCache = PUSH_ARRAY( worldArena, IsoSurfaceSamplingCache, coreThreadsCount );
    world->meshPools = PUSH_ARRAY( worldArena, MeshPool, coreThreadsCount );
    sz arenaAvailable = Available( *worldArena );
    sz maxPerThread = arenaAvailable / 2 / coreThreadsCount;

    // NOTE This limits the max room size we can sample
    const v2u maxVoxelsPerAxis = V2u( 150 );
    for( u32 i = 0; i < coreThreadsCount; ++i )
    {
        world->samplingCache[i] = InitSurfaceSamplingCache( worldArena, maxVoxelsPerAxis );
        InitMeshPool( &world->meshPools[i], worldArena, maxPerThread );
    }

    // Pre-calc offsets to each simulated cluster to pass to shaders
    world->simClusterOffsets = Array<v3>( worldArena, 0, SimRegionSizePerAxis * SimRegionSizePerAxis * SimRegionSizePerAxis + 1 );
    world->simClusterOffsets.Resize( SimRegionSizePerAxis * SimRegionSizePerAxis * SimRegionSizePerAxis + 1 );

    // Use slot 0 for no-offset too, so meshes that don't use this mechanism are not affected
    world->simClusterOffsets[0] = V3Zero;

    for( int k = -SimExteriorHalfSize; k <= SimExteriorHalfSize; ++k )
    {
        r32 zOff = k * ClusterSizeMeters;
        for( int j = -SimExteriorHalfSize; j <= SimExteriorHalfSize; ++j )
        {
            r32 yOff = j * ClusterSizeMeters;
            for( int i = -SimExteriorHalfSize; i <= SimExteriorHalfSize; ++i )
            {
                r32 xOff = i * ClusterSizeMeters;
                u32 index = CalcSimClusterIndex( { i, j, k } );
                world->simClusterOffsets[index] = { xOff, yOff, zOff };
            }
        }
    }

    EndTemporaryMemory( tmpMemory );
}

internal void
AddEntityToCluster( Cluster* cluster, const v3i& clusterP, const v3& entityRelativeP, const v3& entityDim, MeshGeneratorFunc* generatorFunc,
                    const MeshGeneratorData& generatorData )
{
    StoredEntity* newEntity = cluster->entityStorage.PushEmpty();
    newEntity->worldP = { entityRelativeP, clusterP };
    newEntity->dim = entityDim;
    newEntity->generator = { generatorFunc, generatorData };
}

bool SplitVolume( BinaryVolume* v, Array<BinaryVolume>* volumes, const u32 minVolumeSize )
{
    if( v->leftChild || v->rightChild )
        return false;

    v3u dims = v->sizeVoxels;

    // Determine axis of split
    // Split by the dimension which is >25% larger than the others, otherwise split randomly
    u32 maxDimIndex = 0;
    for( u32 d = 1; d < 3; ++d )
    {
        if( dims.e[d] > dims.e[maxDimIndex] )
            maxDimIndex = d;
    }

    u32 remainingDimCount = 3;
    for( u32 d = 0; d < 3; ++d )
    {
        if( (r32)dims.e[maxDimIndex] / dims.e[d] > 1.25f )
        {
            dims.e[d] = 0;
            remainingDimCount--;
        }
    }

    u32 splitDimIndex = 0;
    if( remainingDimCount == 1 )
        splitDimIndex = maxDimIndex;
    else
    {
        // FIXME Random functions should have the seed always passed in!
        splitDimIndex = RandomRangeU32( 0, remainingDimCount - 1 );
        if( dims.e[splitDimIndex] == 0.f )
            splitDimIndex++;
    }

    ASSERT( splitDimIndex < 3 && dims.e[splitDimIndex] > 0.f );

    bool result = false;
    u32 splitSizeMax = dims.e[splitDimIndex] - minVolumeSize;
    if( splitSizeMax > minVolumeSize )
    {
        u32 splitSize = RandomRangeU32( minVolumeSize, splitSizeMax );

        BinaryVolume* left = volumes->PushEmpty();
        left->voxelP = v->voxelP;
        left->sizeVoxels = v->sizeVoxels;
        BinaryVolume* right = volumes->PushEmpty();
        right->voxelP = v->voxelP;
        right->sizeVoxels = v->sizeVoxels;

        switch( splitDimIndex )
        {
            case 0:   // X
            {
                left->sizeVoxels.x = splitSize;
                right->voxelP.x += splitSize;
                right->sizeVoxels.x -= splitSize;
            } break;
            case 1:   // Y
            {
                left->sizeVoxels.y = splitSize;
                right->voxelP.y += splitSize;
                right->sizeVoxels.y -= splitSize;
            } break;
            case 2:   // Z
            {
                left->sizeVoxels.z = splitSize;
                right->voxelP.z += splitSize;
                right->sizeVoxels.z -= splitSize;
            } break;
            INVALID_DEFAULT_CASE
        }

        v->leftChild = left;
        v->rightChild = right;
        result = true;
    }

    return result;
}

internal void
RoomBoundsToMinMaxP( Room const& room, v3u* minP, v3u* maxP )
{
    *minP = room.voxelP;
    *maxP = room.voxelP + room.sizeVoxels - V3uOne;
}

internal void
CreateHall( BinaryVolume* v, Room const& roomA, Room const& roomB, Cluster* cluster )
{
    v3u minP, maxP;

    // Find a random point inside each room
    RoomBoundsToMinMaxP( roomA, &minP, &maxP );
    v->hall.startP =
    {
        RandomRangeU32( minP.x, maxP.x ),
        RandomRangeU32( minP.y, maxP.y ),
        RandomRangeU32( minP.z, maxP.z ),
    };
    RoomBoundsToMinMaxP( roomB, &minP, &maxP );
    v->hall.endP =
    {
        RandomRangeU32( minP.x, maxP.x ),
        RandomRangeU32( minP.y, maxP.y ),
        RandomRangeU32( minP.z, maxP.z ),
    };

    // Walk along each axis in a random order
    v3u& startP = v->hall.startP;
    v3u& endP = v->hall.endP;
    v3u currentP = startP;

    u8 axisOrder = 0, remainingAxes = 3;
    int nextAxis[] = { 0, 1, 2 };
    while( remainingAxes )
    {
        u32 index = RandomRangeU32( 0, 2 );

        if( nextAxis[index] == -1 )
            continue;
        else
        {
            int& next = nextAxis[index];
            switch( next )
            {
                case 0:   // X
                {
                    u32 start = Min( currentP.x, endP.x );
                    u32 end = Max( currentP.x, endP.x );
                    u32 j = currentP.y;
                    u32 k = currentP.z;

                    for( u32 i = start; i <= end; ++i)
                        cluster->voxelGrid( i, j, k ) = 3;

                    currentP.x = endP.x;
                } break;
                case 1:   // Y
                {
                    u32 start = Min( currentP.y, endP.y );
                    u32 end = Max( currentP.y, endP.y );
                    u32 i = currentP.x;
                    u32 k = currentP.z;

                    for( u32 j = start; j <= end; ++j)
                        cluster->voxelGrid( i, j, k ) = 3;

                    currentP.y = endP.y;
                } break;
                case 2:   // Z
                {
                    u32 start = Min( currentP.z, endP.z );
                    u32 end = Max( currentP.z, endP.z );
                    u32 i = currentP.x;
                    u32 j = currentP.y;

                    for( u32 k = start; k <= end; ++k)
                        cluster->voxelGrid( i, j, k ) = 3;

                    currentP.z = endP.z;
                } break;
                INVALID_DEFAULT_CASE
            }

            // Record which way we went
            int bitOffset = (3 - remainingAxes) * 2;
            v->hall.axisOrder |= ((next & 0x3) << bitOffset);

            next = -1;
            remainingAxes--;
        }
    }
}

internal Room*
CreateRooms( BinaryVolume* v, SectorParams const& genParams, Cluster* cluster, v3i const& clusterP,
             IsoSurfaceSamplingCache* samplingCache, MeshPool* meshPool, World* world )
{
    if( v->leftChild || v->rightChild )
    {
        Room* leftRoom = nullptr;
        Room* rightRoom = nullptr;

        if( v->leftChild )
            leftRoom = CreateRooms( v->leftChild, genParams, cluster, clusterP, samplingCache, meshPool, world );
        if( v->rightChild )
            rightRoom = CreateRooms( v->rightChild, genParams, cluster, clusterP, samplingCache, meshPool, world ); 

        if( leftRoom && rightRoom )
            CreateHall( v, *leftRoom, *rightRoom, cluster );

        return RandomNormalizedR32() < 0.5f ? leftRoom : rightRoom;
    }
    else
    {
        v3u const& vSize = v->sizeVoxels;

        v3u roomSizeVoxels =
        {
            RandomRangeU32( (u32)(vSize.x * genParams.minRoomSizeRatio), (u32)(vSize.x * genParams.maxRoomSizeRatio) ),
            RandomRangeU32( (u32)(vSize.y * genParams.minRoomSizeRatio), (u32)(vSize.y * genParams.maxRoomSizeRatio) ),
            RandomRangeU32( (u32)(vSize.z * genParams.minRoomSizeRatio), (u32)(vSize.z * genParams.maxRoomSizeRatio) ),
        };

        v3u roomOffset =
        {
            RandomRangeU32( genParams.volumeSafeMarginSize, vSize.x - roomSizeVoxels.x - genParams.volumeSafeMarginSize ),
            RandomRangeU32( genParams.volumeSafeMarginSize, vSize.y - roomSizeVoxels.y - genParams.volumeSafeMarginSize ),
            RandomRangeU32( genParams.volumeSafeMarginSize, vSize.z - roomSizeVoxels.z - genParams.volumeSafeMarginSize ),
        };

        const v3 clusterHalfSizeMeters = V3( ClusterSizeMeters * 0.5f );

        v3u roomIntMinP = v->voxelP + roomOffset;
        v3u roomIntMaxP = roomIntMinP + roomSizeVoxels - V3uOne;

        v->room.voxelP = roomIntMinP;
        v->room.sizeVoxels = roomSizeVoxels;
        v->room.worldP = { -clusterHalfSizeMeters + (V3( roomIntMinP ) + V3( roomSizeVoxels ) * 0.5f) * VoxelSizeMeters, clusterP };
        v->room.halfSize = V3( roomSizeVoxels ) * 0.5f * VoxelSizeMeters;

#if 0
        for( u32 k = roomIntMinP.z; k <= roomIntMaxP.z; ++k )
            for( u32 j = roomIntMinP.y; j <= roomIntMaxP.y; ++j )
                for( u32 i = roomIntMinP.x; i <= roomIntMaxP.x; ++i )
                {
                    bool atBorder = (i == roomIntMinP.x || i == roomIntMaxP.x)
                        || (j == roomIntMinP.y || j == roomIntMaxP.y)
                        || (k == roomIntMinP.z || k == roomIntMaxP.z);
                    cluster->voxelGrid( i, j, k ) = atBorder ? 2 : 1;
                }
#else
        // FIXME Account for sample volume thickness during room volume calculations
        u32 volumeShellThickness = 3;
        v3u roomExtP = roomIntMinP - V3u( volumeShellThickness );
        v3u roomExtSize = roomSizeVoxels + V3u( 2 * volumeShellThickness );

        v2u voxelsPerSliceAxis = roomExtSize.xy;
        v2u gridEdgesPerSliceAxis = voxelsPerSliceAxis + V2uOne;

        WorldCoords worldP = { V3Zero, clusterP };
        // Just pass the room data for now
        void* const roomSamplingData = &v->room;

        ASSERT( samplingCache->cellsPerAxis.x >= voxelsPerSliceAxis.x && samplingCache->cellsPerAxis.y >= voxelsPerSliceAxis.y );

        // TODO Do all this in jobs in LoadEntitiesInCluster
        ClearScratchBuffers( meshPool );

        // TODO Super sample the volume shell?
        // TODO Skip interior!
        bool firstSlice = true;
        for( u32 k = 0; k < roomExtSize.z; ++k )
        {
            // TODO Would be interesting to study how we could sample _all rooms in the cluster at once_ slice by slice,
            // while keeping their meshes separate

            // Pre-sample bottom and top corners of cubes for the first slice, only the top ones for the rest
            for( u32 n = 0; n < 2; ++n )
            {
                if( n == 0 && !firstSlice )
                    continue;

                worldP.relativeP = -clusterHalfSizeMeters + V3( roomExtP + V3u( 0, 0, k + n ) ) * VoxelSizeMeters;

                r32* sample = n ? samplingCache->topLayerSamples : samplingCache->bottomLayerSamples;
                // Iterate grid lines when sampling each layer, since we need to have samples at the extremes too
                for( u32 j = 0; j < gridEdgesPerSliceAxis.y; ++j )
                {
                    v3 pAtRowStart = worldP.relativeP;
                    for( u32 i = 0; i < gridEdgesPerSliceAxis.x; ++i )
                    {
                        *sample++ = RoomSurfaceFunc( roomSamplingData, worldP );
                        worldP.relativeP.x += VoxelSizeMeters;
                    }
                    worldP.relativeP = pAtRowStart;
                    worldP.relativeP.y += VoxelSizeMeters;
                }
            }

            // Keep a cache of already calculated vertices to eliminate duplication
            ClearVertexCaches( samplingCache, firstSlice );

            worldP.relativeP = -clusterHalfSizeMeters + V3( roomExtP + V3u( 0, 0, k ) ) * VoxelSizeMeters;
            for( u32 j = 0; j < voxelsPerSliceAxis.y; ++j )
            {
                v3 pAtRowStart = worldP.relativeP;
                for( u32 i = 0; i < voxelsPerSliceAxis.x; ++i )
                {
                    MarchCube( worldP.relativeP, V2i( i, j ), voxelsPerSliceAxis, VoxelSizeMeters, samplingCache,
                               &meshPool->scratchVertices, &meshPool->scratchIndices );
                    worldP.relativeP.x += VoxelSizeMeters;
                }
                worldP.relativeP = pAtRowStart;
                worldP.relativeP.y += VoxelSizeMeters;
            }

            SwapTopAndBottomLayers( samplingCache );
            firstSlice = false;
        }

        // Write output mesh
        v->room.mesh = AllocateMeshFromScratchBuffers( meshPool );
        // Set offset index based on cluster
        v3i clusterRelativeP = clusterP - world->originClusterP;
        v->room.mesh->simClusterIndex = CalcSimClusterIndex( clusterRelativeP );
#endif

        cluster->rooms.Push( v->room );
        return &v->room;
    }
}

internal v3
GetClusterOffsetFromOrigin( const v3i& clusterP, const v3i& worldOriginClusterP )
{
    v3 result = V3(clusterP - worldOriginClusterP) * ClusterSizeMeters ;
    return result;
}

internal bool
IsInSimRegion( const v3i& clusterP, const v3i& worldOriginClusterP )
{
    v3i clusterOffset = clusterP - worldOriginClusterP;

    return clusterOffset.x >= -SimExteriorHalfSize && clusterOffset.x <= SimExteriorHalfSize
        && clusterOffset.y >= -SimExteriorHalfSize && clusterOffset.y <= SimExteriorHalfSize
        && clusterOffset.z >= -SimExteriorHalfSize && clusterOffset.z <= SimExteriorHalfSize ;
}

internal void
CreateEntitiesInCluster( Cluster* cluster, const v3i& clusterP, World* world, MemoryArena* arena )
{
    // Partition cluster space
    SectorParams genParams = CollectSectorParams( clusterP );
    const u32 minVolumeSize = (u32)(genParams.minVolumeRatio * (r32)VoxelsPerClusterAxis);
    const u32 maxVolumeSize = (u32)(genParams.maxVolumeRatio * (r32)VoxelsPerClusterAxis);

    // TODO Calc an upper bound given cluster size and minimum volume size
    const u32 maxSplits = 4096;
    //Array<BinaryVolume> volumes = Array<BinaryVolume>( arena, 0, maxSplits );
    Array<BinaryVolume>& volumes = cluster->volumes;

    BinaryVolume *rootVolume = volumes.PushEmpty();
    rootVolume->voxelP = V3uZero;
    rootVolume->sizeVoxels = V3u( VoxelsPerClusterAxis );


    bool didSplit = true;
    // Dont retry volumes that didn't split the first time, do the whole thing in one go
    //while( didSplit )
    {
        didSplit = false;
        // Iterate as we add more stuff
        for( u32 i = 0; i < volumes.count; ++i )
        {
            BinaryVolume& v = volumes[i];

            if( v.leftChild == nullptr && v.rightChild == nullptr )
            {
                // If this volume is too big, or a certain chance...
                if( v.sizeVoxels.x > maxVolumeSize ||
                    v.sizeVoxels.y > maxVolumeSize ||
                    v.sizeVoxels.z > maxVolumeSize ||
                    RandomNormalizedR32() > genParams.volumeExtraPartitioningProbability )
                {
                    if( SplitVolume( &v, &volumes, minVolumeSize ) )
                        didSplit = true;
                }
            }
        }
    }

    u32 totalVolumes = volumes.count;

    // TODO Make it sparse! (octree)
    cluster->voxelGrid = ClusterVoxelGrid( arena, V3u( VoxelsPerClusterAxis ) );

    v3 clusterGridWorldP = GetClusterOffsetFromOrigin( clusterP, world->originClusterP ) - V3( ClusterSizeMeters * 0.5f );
    // Create a room in each volume
    // TODO Add a certain chance for empty volumes
    CreateRooms( rootVolume, genParams, cluster, clusterP, world->samplingCache, &world->meshPools[0], world );
}

inline MeshGeneratorJob*
FindFreeJob( World* world )
{
    bool found = false;
    MeshGeneratorJob* result = nullptr;

    u32 index = world->lastAddedJob;

    do
    {
        result = &world->generatorJobs[index];
        if( !result->occupied )
        {
            found = true;
            world->lastAddedJob = index;
            break;
        }

        index = (index + 1) % PLATFORM_MAX_JOBQUEUE_JOBS;
    }
    while( index != world->lastAddedJob );

    ASSERT( found );
    return result;
}

internal
PLATFORM_JOBQUEUE_CALLBACK(GenerateOneEntity)
{
    MeshGeneratorJob* job = (MeshGeneratorJob*)userData;

    const v3i& clusterP = job->storedEntity->worldP.clusterP;

    // TODO Use CAS whenever worldOriginClusterP changes
    if( IsInSimRegion( clusterP, *job->worldOriginClusterP ) )
    {
        // TODO Find a much more explicit and general way to associate thread-job data like this
        IsoSurfaceSamplingCache* samplingCache = &job->samplingCache[workerThreadIndex];
        MeshPool* meshPool = &job->meshPools[workerThreadIndex];

        // Make live entity from stored and put it in the world
        Mesh* generatedMesh = job->storedEntity->generator.func( job->storedEntity->generator.data, job->storedEntity->worldP,
                                                                 samplingCache, meshPool );
        *job->outputEntity =
        {
            *job->storedEntity,
            generatedMesh,
            EntityState::Loaded,
        };
    }

    // TODO Review how to synchronize this
    MEMORY_WRITE_BARRIER    

    job->occupied = false;
}

internal void
LoadEntitiesInCluster( const v3i& clusterP, World* world, MemoryArena* arena )
{
    TIMED_BLOCK;

    Cluster* cluster = world->clusterTable.Find( clusterP );

    if( !cluster )
    {
        cluster = world->clusterTable.InsertEmpty( clusterP );
        cluster->populated = false;
        new (&cluster->entityStorage) BucketArray<StoredEntity>( arena, 256 );
        const u32 maxSplits = 256;
        cluster->volumes = Array<BinaryVolume>( arena, 0, maxSplits );
        cluster->rooms = Array<Room>( arena, 0, 32 );
    }

    if( !cluster->populated )
    {
        CreateEntitiesInCluster( cluster, clusterP, world, arena );
        cluster->populated = true;
    }


#if 0
    {
        TIMED_BLOCK;

        BucketArray<StoredEntity>::Idx it = cluster->entityStorage.First();
        // TODO Pre-reserve a bunch of slots and generate entities bundles and measure
        // if there's any speed difference
        while( it )
        {
            StoredEntity& storedEntity = it;
            LiveEntity* outputEntity = world->liveEntities.PushEmpty();

            if( storedEntity.generator.func )
            {
                outputEntity->state = EntityState::Invalid;


                // Start new job in a hi priority thread
                MeshGeneratorJob* job = FindFreeJob( world );
                *job =
                {
                    &storedEntity,
                    &world->originClusterP,
                    world->samplingCache,
                    world->meshPools,
                    outputEntity,
                };
                job->occupied = true;

                globalPlatform.AddNewJob( globalPlatform.hiPriorityQueue,
                                          GenerateOneEntity,
                                          job );
            }
            else
            {
                *outputEntity =
                {
                    storedEntity,
                    nullptr,
                    EntityState::Loaded,
                };
            }

            it.Next();
        }
    }
#endif
}

internal void
StoreEntitiesInCluster( const v3i& clusterP, World* world, MemoryArena* arena )
{
    TIMED_BLOCK;

    Cluster* cluster = world->clusterTable.Find( clusterP );

    if( !cluster )
    {
        // Should only happen on the very first iteration
        ASSERT( world->clusterTable.count == 0 );
        return;
    }

    cluster->entityStorage.Clear();
    v3 clusterWorldOffset = GetClusterOffsetFromOrigin( clusterP, world->originClusterP );

    r32 clusterHalfSize = ClusterSizeMeters  / 2.f;

    BucketArray<LiveEntity>::Idx it = world->liveEntities.Last();
    while( it )
    {
        LiveEntity& liveEntity = ((LiveEntity&)it);
        if( liveEntity.mesh )
        {
            v3 entityRelativeP = GetTranslation( liveEntity.mesh->mTransform ) - clusterWorldOffset;

            if( entityRelativeP.x > -clusterHalfSize && entityRelativeP.x < clusterHalfSize &&
                entityRelativeP.y > -clusterHalfSize && entityRelativeP.y < clusterHalfSize &&
                entityRelativeP.z > -clusterHalfSize && entityRelativeP.z < clusterHalfSize )
            {
                StoredEntity& storedEntity = liveEntity.stored;
                storedEntity.worldP.clusterP = clusterP;
                storedEntity.worldP.relativeP = entityRelativeP;

                cluster->entityStorage.Push( storedEntity );

                ReleaseMesh( &liveEntity.mesh );
                world->liveEntities.Remove( it );
            }
        }

        it.Prev();
    }
}

// @Leak
internal void
RestartWorldGeneration( World* world )
{
    // TODO 
    RandomSeed();

    world->liveEntities.Clear();
    world->clusterTable.Clear();

    world->originClusterP = V3iZero;
    world->lastOriginClusterP = INITIAL_CLUSTER_COORDS;
}

internal void
UpdateWorldGeneration( GameInput* input, World* world, MemoryArena* arena )
{
    TIMED_BLOCK;

    // TODO Make an infinite connected 'cosmic grid structure'
    // so we can test for a good cluster size, evaluate current generation speeds,
    // debug moving across clusters, etc.

#if !RELEASE
    if( input->gameCodeReloaded )
        RestartWorldGeneration( world );
#endif

    if( world->originClusterP != world->lastOriginClusterP )
    {
        v3 vWorldDelta = (world->lastOriginClusterP - world->originClusterP) * ClusterSizeMeters ;

        // Offset all live entities by the world delta
        // NOTE This could be done more efficiently _after_ storing entities
        // (but we'd have to account for the delta in StoreEntitiesInCluster)
        auto it = world->liveEntities.First();
        while( it )
        {
            LiveEntity& liveEntity = ((LiveEntity&)it);

            if( liveEntity.mesh )
                Translate( liveEntity.mesh->mTransform, vWorldDelta );

            it.Next();
        }
        // TODO Should we put the player(s) in the live entities table?
        if( world->lastOriginClusterP != INITIAL_CLUSTER_COORDS )
            world->pPlayer += vWorldDelta;

        for( int i = -SimExteriorHalfSize; i <= SimExteriorHalfSize; ++i )
        {
            for( int j = -SimExteriorHalfSize; j <= SimExteriorHalfSize; ++j )
            {
                for( int k = -SimExteriorHalfSize; k <= SimExteriorHalfSize; ++k )
                {
                    v3i lastClusterP = world->lastOriginClusterP + V3i( i, j, k );

                    // Evict all entities contained in a cluster which is now out of bounds
                    if( !IsInSimRegion( lastClusterP, world->originClusterP ) )
                    {
                        // FIXME This is very dumb!
                        // We first iterate clusters and then iterate the whole liveEntities
                        // filtering by that cluster.. ¬¬ Just iterate liveEntities once and discard
                        // all which are not valid, and use the integer cluster coordinates to do so!
                        // (Just call IsInSimRegion())
                        // Also! Don't forget to check the cluster an entity belongs to
                        // everytime you move them!
                        StoreEntitiesInCluster( lastClusterP, world, arena );
                    }
                }
            }
        }

        for( int i = -SimExteriorHalfSize; i <= SimExteriorHalfSize; ++i )
        {
            for( int j = -SimExteriorHalfSize; j <= SimExteriorHalfSize; ++j )
            {
                for( int k = -SimExteriorHalfSize; k <= SimExteriorHalfSize; ++k )
                {
                    v3i clusterP = world->originClusterP + V3i( i, j, k );

                    // Retrieve all entities contained in a cluster which is now inside bounds
                    // and put them in the live entities list
                    if( !IsInSimRegion( clusterP, world->lastOriginClusterP ) )
                    {
                        LoadEntitiesInCluster( clusterP, world, arena );
                    }
                }
            }
        }
    }
    world->lastOriginClusterP = world->originClusterP;


    {
        TIMED_BLOCK;

        // Constantly monitor live entities array for inactive entities
        auto it = world->liveEntities.First();
        while( it )
        {
            LiveEntity& entity = ((LiveEntity&)it);
            if( entity.state == EntityState::Loaded )
            {
                if( entity.mesh )
                {
                    v3 clusterWorldOffset = GetClusterOffsetFromOrigin( entity.stored.worldP.clusterP, world->originClusterP );
                    Translate( entity.mesh->mTransform, clusterWorldOffset );
                }

                entity.state = EntityState::Active;
            }
            it.Next();
        }
    }
}

internal v3
GetLiveEntityWorldP( const WorldCoords& coords, const v3i& worldOriginClusterP )
{
    v3 clusterOffset = GetClusterOffsetFromOrigin( coords.clusterP, worldOriginClusterP );
    v3 result = clusterOffset + coords.relativeP;
    return result;
}

void
UpdateAndRenderWorld( GameInput *input, GameMemory* gameMemory, RenderCommands *renderCommands )
{
    TIMED_BLOCK;

    GameState *gameState = (GameState*)gameMemory->permanentStorage;

    float dT = input->frameElapsedSeconds;
    float elapsedT = input->totalElapsedSeconds;
    World* world = gameState->world;

    ///// Update

#if !RELEASE
    DebugState* debugState = (DebugState*)gameMemory->debugStorage;
#endif

    // Update player based on input
    GameControllerInput *input1 = GetController( input, 1 );
    if( input1->isConnected )
    {
        // Ready player 1
        Player *player = world->player;
        v3 pPlayer = world->pPlayer;

        r32 translationSpeed = input1->leftShoulder.endedDown ? 50.f : 25.f;
        v3 vPlayerDelta = {};

        if( input1->leftStick.avgX != 0 )
        {
            vPlayerDelta.x += input1->leftStick.avgX * translationSpeed * dT;
        }
        if( input1->leftStick.avgY != 0 )
        {
            vPlayerDelta.y += input1->leftStick.avgY * translationSpeed * dT;
        }

        r32 rotationSpeed = 0.1f;
        if( input1->rightStick.avgX != 0 )
        {
            world->playerYaw += -input1->rightStick.avgX * rotationSpeed * dT; 
        }
        if( input1->rightStick.avgY != 0 )
        {
            world->playerPitch += -input1->rightStick.avgY * rotationSpeed * dT; 
        }

        m4 mPlayerRot = M4ZRotation( world->playerYaw ) * M4XRotation( world->playerPitch );
        pPlayer = pPlayer + mPlayerRot * vPlayerDelta;
        player->mesh.mTransform = M4RotPos( mPlayerRot, pPlayer );

        world->pPlayer = pPlayer;

        // Check if we moved to a different cluster
        {
            v3i originClusterP = world->originClusterP;
            r32 clusterHalfSize = ClusterSizeMeters  / 2.f;

            if( pPlayer.x > clusterHalfSize )
                originClusterP.x++;
            else if( pPlayer.x < -clusterHalfSize )
                originClusterP.x--;
            if( pPlayer.y > clusterHalfSize )
                originClusterP.y++;
            else if( pPlayer.y < -clusterHalfSize )
                originClusterP.y--;
            if( pPlayer.z > clusterHalfSize )
                originClusterP.z++;
            else if( pPlayer.z < -clusterHalfSize )
                originClusterP.z--;

            if( originClusterP != world->originClusterP )
                world->originClusterP = originClusterP;
        }
    }


    //UpdateWorldGeneration( input, world, &gameState->worldArena );




    ///// Render
    RenderClear( { 0.99f, 0.95f, 0.9f, 1.0f }, renderCommands );

    // Pass world cluster offsets into the commands
    renderCommands->simClusterOffsets = world->simClusterOffsets.data;
    renderCommands->simClusterCount = world->simClusterOffsets.count;

#if !RELEASE
    if( !gameMemory->DEBUGglobalEditing )
#endif
    {
        RenderSetShader( ShaderProgramName::FlatShading, renderCommands );

        auto it = world->liveEntities.First();
        while( it )
        {
            TIMED_BLOCK;

            LiveEntity& entity = (LiveEntity&)it;
            if( entity.state == EntityState::Active )
            {
                if( entity.mesh )
                    RenderMesh( *entity.mesh, renderCommands );
            }

            it.Next();
        }

        RenderSetShader( ShaderProgramName::PlainColor, renderCommands );
        RenderSetMaterial( nullptr, renderCommands );
        u32 black = Pack01ToRGBA( 0, 0, 0, 1 );

        it = world->liveEntities.First();
        while( it )
        {
            TIMED_BLOCK;

            LiveEntity& entity = (LiveEntity&)it;
            if( entity.state == EntityState::Active )
            {
                v3 entityP = GetLiveEntityWorldP( entity.stored.worldP, world->originClusterP );
                aabb entityBounds = AABB( entityP, entity.stored.dim );
                RenderBounds( entityBounds, black, renderCommands );
            }

            it.Next();
        }
#if !RELEASE
        debugState->totalEntities = world->liveEntities.count;
#endif
    }

    // Render current cluster limits
    RenderSetMaterial( nullptr, renderCommands );
    RenderSetShader( ShaderProgramName::PlainColor, renderCommands );
    u32 red = Pack01ToRGBA( 1, 0, 0, 1 );
    RenderBoundsAt( V3Zero, ClusterSizeMeters, red, renderCommands );
    //RenderBoxAt( V3Zero, ClusterSizeMeters, red, renderCommands );

    if( !gameMemory->DEBUGglobalEditing )
    {
        RenderSetMaterial( world->player->mesh.material, renderCommands );
        RenderMesh( world->player->mesh, renderCommands );
        RenderSetMaterial( nullptr, renderCommands );

#if 1
        Cluster* currentCluster = world->clusterTable.Find( world->originClusterP );
        r32 clusterHalfSize = ClusterSizeMeters * 0.5f;
        u32 halfRed = Pack01ToRGBA( 1, 0, 0, 0.2f );

        // Render partitioned volumes
        {
            // Volume bounds are in voxel units
            for( u32 i = 0; i < currentCluster->volumes.count; ++i )
            {
                BinaryVolume& v = currentCluster->volumes[i];
                if( v.leftChild == nullptr && v.rightChild == nullptr )
                {
                    v3 minBounds = V3( v.voxelP );
                    v3 maxBounds = V3( v.voxelP + v.sizeVoxels );
                    aabb worldBounds =
                    {
                        { minBounds * VoxelSizeMeters - V3( clusterHalfSize ) },
                        { maxBounds * VoxelSizeMeters - V3( clusterHalfSize ) },
                    };
                    RenderBounds( worldBounds, halfRed, renderCommands );
                }
            }
        }

        RenderSetShader( ShaderProgramName::FlatShading, renderCommands );

        for( int i = -SimExteriorHalfSize; i <= SimExteriorHalfSize; ++i )
        {
            for( int j = -SimExteriorHalfSize; j <= SimExteriorHalfSize; ++j )
            {
                for( int k = -SimExteriorHalfSize; k <= SimExteriorHalfSize; ++k )
                {
                    v3i clusterP = world->originClusterP + V3i( i, j, k );
                    Cluster* cluster = world->clusterTable.Find( clusterP );

                    for( u32 r = 0; r < cluster->rooms.count; ++r )
                    {
                        Room& room = cluster->rooms[r];
                        RenderMesh( *room.mesh, renderCommands );
                    }
                }
            }
        }
#endif
    }

    {
        // Create a chasing camera
        // TODO Use a PID controller
        Player *player = world->player;
        v3 pCam = player->mesh.mTransform * V3( 0, -25, 10 );
        v3 pLookAt = player->mesh.mTransform * V3( 0, 1, 0 );
        v3 vUp = GetColumn( player->mesh.mTransform, 2 ).xyz;
        renderCommands->camera = DefaultCamera();
        renderCommands->camera.worldToCamera = M4CameraLookAt( pCam, pLookAt, vUp );
    }    
}
