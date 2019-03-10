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

#define INITIAL_CLUSTER_COORDS V3i( I32MAX, I32MAX, I32MAX )

void
InitWorld( World* world, MemoryArena* worldArena, MemoryArena* transientArena )
{
    // Is this really necessary now?
    ASSERT( (r32)(u32)(VoxelsPerClusterAxis * VoxelSizeMeters) == ClusterSizeMeters );

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

    world->marchingAreaSize = 100;
    world->marchingCubeSize = 5;
    r32 cellsPerAxis = world->marchingAreaSize / world->marchingCubeSize;
    ASSERT( cellsPerAxis == (u32)cellsPerAxis );

    world->cacheBuffers = PUSH_ARRAY( worldArena, MarchingCacheBuffers, globalPlatform.coreThreadsCount );
    world->meshPools = PUSH_ARRAY( worldArena, MeshPool, globalPlatform.coreThreadsCount );
    sz arenaAvailable = Available( *worldArena );
    sz maxPerThread = arenaAvailable / 2 / globalPlatform.coreThreadsCount;
    for( u32 i = 0; i < globalPlatform.coreThreadsCount; ++i )
    {
        world->cacheBuffers[i] = InitMarchingCacheBuffers( worldArena, (u32)cellsPerAxis );
        Init( &world->meshPools[i], worldArena, maxPerThread );
    }

    EndTemporaryMemory( tmpMemory );
}

internal void
AddEntityToCluster( Cluster* cluster, const v3i& clusterP, const v3& entityRelativeP, const v3& entityDim, MeshGeneratorFunc* generatorFunc,
                    const MeshGeneratorData& generatorData )
{
    StoredEntity* newEntity = cluster->entityStorage.PushEmpty();
    newEntity->coords = { clusterP, entityRelativeP };
    newEntity->dim = entityDim;
    newEntity->generator = { generatorFunc, generatorData };
}

internal void
CreateEntitiesInCluster( Cluster* cluster, const v3i& clusterP, World* world, MemoryArena* arena )
{
#if 0
    const r32 margin = 3.f;
    const r32 step = 50.f;

    // Place an entity every few meters, leaving some margin
    r32 clusterHalfSize = ClusterSizeMeters  / 2.f;
    for( r32 x = -clusterHalfSize + margin; x <= clusterHalfSize - margin; x += step )
    {
        for( r32 y = -clusterHalfSize + margin; y <= clusterHalfSize - margin; y += step )
        {
            for( r32 z = -clusterHalfSize + margin; z <= clusterHalfSize - margin; z += step )
            {
                StoredEntity newEntity =
                {
                    { clusterP, V3( x, y, z ) },
                    // TODO Think of how to assign specific generators to the created entities
                    &meshGenerators[0],
                };
                cluster->entityStorage.Add( newEntity );
            }
        }
    }
#endif

    // Partition cluster space
    // NOTE This will be transient but we'll keep it for now for debugging
    //TemporaryMemory tmpMemory = BeginTemporaryMemory( arena );

    v2i clusterSpan = { 0, VoxelsPerClusterAxis };
    aabbi rootBounds = AABBi( clusterSpan, clusterSpan, clusterSpan );
    Volume rootVolume = { rootBounds };

    SectorParams params = CollectSectorParams( clusterP );
    i32 minVolumeSize = (i32)(params.minVolumeRatio * (r32)VoxelsPerClusterAxis);
    i32 maxVolumeSize = (i32)(params.maxVolumeRatio * (r32)VoxelsPerClusterAxis);

    // TODO Calc an upper bound given cluster size and minimal volume size
    u32 maxSplits = 4096;
    //Array<Volume> volumes = Array<Volume>( arena, 0, maxSplits );
    cluster->volumes = Array<Volume>( arena, 0, maxSplits );
    Array<Volume>& volumes = cluster->volumes;

    volumes.Push( rootVolume );
    bool didSplit = true;

    while( didSplit )
    {
        didSplit = false;
        for( u32 i = 0; i < volumes.count; ++i )
        {
            Volume& v = volumes[i];

            if( v.leftChild == nullptr && v.rightChild == nullptr )
			{
                v3i boundsDim;
                XYZSize( v.bounds, &boundsDim.x, &boundsDim.y, &boundsDim.z );

                // If this volume is too big, or a certain chance...
                if( boundsDim.x > maxVolumeSize ||
                    boundsDim.y > maxVolumeSize ||
                    boundsDim.z > maxVolumeSize ||
                    RandomNormalizedR32() > params.volumeExtraPartitioningProbability )
                {
                    u32 splitDimIndex = 0;

                    // Split by the dimension which is >25% larger than the others, otherwise split randomly
                    u32 maxSizeIndex = 0;
                    u32 remainingDimCount = 3;

                    for( u32 d = 0; d < 3; ++d )
                    {
                        if( boundsDim.e[d] > boundsDim.e[maxSizeIndex] )
                        {
                            maxSizeIndex = d;
                        }
                    }

                    for( u32 d = 0; d < 3; ++d )
                    {
                        if( boundsDim.e[d] < boundsDim.e[maxSizeIndex] * 0.75f )
                        {
                            boundsDim.e[d] = 0;
                            remainingDimCount--;
                        }
                    }

                    ASSERT( remainingDimCount );
                    if( remainingDimCount == 1 )
                        splitDimIndex = maxSizeIndex;
                    else
                    {
                        splitDimIndex = RandomRangeU32( 0, remainingDimCount - 1 );
                        if( boundsDim.e[splitDimIndex] == 0.f )
                            splitDimIndex++;
                    }

                    ASSERT( splitDimIndex < 3 && boundsDim.e[splitDimIndex] > 0.f );

                    // TODO Really start considering ditching unsigneds!!
                    i32 splitSizeMax = boundsDim.e[splitDimIndex] - minVolumeSize;
                    if( splitSizeMax > minVolumeSize )
                    {
                        i32 splitSize = RandomRangeI32( minVolumeSize, splitSizeMax );
                        // TODO Really start considering ditching unsigneds!!
                        i32 splitSizeI32 = (i32)splitSize;
                        aabbi left = v.bounds, right = v.bounds;

                        switch( splitDimIndex )
                        {
                            case 0:   // X
                            {
                                left.xSpan = { v.bounds.xMin, v.bounds.xMin + splitSizeI32 };
                                right.xSpan = { v.bounds.xMin + splitSizeI32 + 1, v.bounds.xMax };
                            } break;
                            case 1:   // Y
                            {
                                left.ySpan = { v.bounds.yMin, v.bounds.yMin + splitSizeI32 };
                                right.ySpan = { v.bounds.yMin + splitSizeI32 + 1, v.bounds.yMax };
                            } break;
                            case 2:   // Z
                            {
                                left.zSpan = { v.bounds.zMin, v.bounds.zMin + splitSizeI32 };
                                right.zSpan = { v.bounds.zMin + splitSizeI32 + 1, v.bounds.zMax };
                            } break;
                            INVALID_DEFAULT_CASE
                        }

                        v.leftChild = volumes.PushEmpty();
                        *v.leftChild = { left };

                        v.rightChild = volumes.PushEmpty();
                        *v.rightChild = { right };

                        didSplit = true;
                    }
                }
            }
        }
    }

    // TODO Make it sparse!
    cluster->voxelGrid = (ClusterVoxelLayer*)PUSH_ARRAY( arena, u8, VoxelsPerClusterAxis * VoxelsPerClusterAxis * VoxelsPerClusterAxis );

    // Create a room in each volume
    for( u32 idx = 0; idx < volumes.count; ++idx )
    {
        Volume& v = volumes[idx];
        if( v.leftChild == nullptr && v.rightChild == nullptr )
        {
            v3i vDim;
            XYZSize( v.bounds, &vDim.x, &vDim.y, &vDim.z );

            v3i roomDim =
            {
                RandomRangeI32( (i32)(vDim.x * params.minRoomSizeRatio), (i32)(vDim.x * params.maxRoomSizeRatio) ),
                RandomRangeI32( (i32)(vDim.y * params.minRoomSizeRatio), (i32)(vDim.y * params.maxRoomSizeRatio) ),
                RandomRangeI32( (i32)(vDim.z * params.minRoomSizeRatio), (i32)(vDim.z * params.maxRoomSizeRatio) ),
            };

            v3i roomOffset =
            {
                RandomRangeI32( params.volumeSafeMarginSize, vDim.x - roomDim.x - params.volumeSafeMarginSize ),
                RandomRangeI32( params.volumeSafeMarginSize, vDim.y - roomDim.y - params.volumeSafeMarginSize ),
                RandomRangeI32( params.volumeSafeMarginSize, vDim.z - roomDim.z - params.volumeSafeMarginSize ),
            };

            aabbi roomBounds =
            {
                v.bounds.xMin + roomOffset.x, v.bounds.xMin + roomOffset.x + roomDim.x,
                v.bounds.yMin + roomOffset.y, v.bounds.yMin + roomOffset.y + roomDim.y,
                v.bounds.zMin + roomOffset.z, v.bounds.zMin + roomOffset.z + roomDim.z,
            };

            for( int i = roomBounds.xMin; i <= roomBounds.xMax; ++i )
                for( int j = roomBounds.yMin; j <= roomBounds.yMax; ++j )
                    for( int k = roomBounds.zMin; k <= roomBounds.zMax; ++k )
                        cluster->voxelGrid[i][j][k] = 1;
        }
    }

    //EndTemporaryMemory( tmpMemory );
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

    return clusterOffset.x >= -SimRegionWidth  && clusterOffset.x <= SimRegionWidth  &&
        clusterOffset.y >= -SimRegionWidth  && clusterOffset.y <= SimRegionWidth  &&
        clusterOffset.z >= -SimRegionWidth  && clusterOffset.z <= SimRegionWidth ;
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

    const v3i& clusterP = job->storedEntity->coords.clusterP;

    // TODO Review how to synchronize this
    if( IsInSimRegion( clusterP, *job->worldOriginClusterP ) )
    {
        // TODO Find a much more explicit and general way to associate thread-job data like this
        MarchingCacheBuffers* cacheBuffers = &job->cacheBuffers[workerThreadIndex];
        MeshPool* meshPool = &job->meshPools[workerThreadIndex];

        // Make live entity from stored and put it in the world
        Mesh* generatedMesh = job->storedEntity->generator.func( job->storedEntity->generator.data, job->storedEntity->coords, cacheBuffers, meshPool );
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
        cluster = world->clusterTable.Reserve( clusterP );
        cluster->populated = false;
        new (&cluster->entityStorage) BucketArray<StoredEntity>( arena, 256 );
    }

    if( !cluster->populated )
    {
        CreateEntitiesInCluster( cluster, clusterP, world, arena );
        cluster->populated = true;
    }


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
                    world->cacheBuffers,
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
                storedEntity.coords.clusterP = clusterP;
                storedEntity.coords.relativeP = entityRelativeP;

                cluster->entityStorage.Add( storedEntity );

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

        for( int i = -SimRegionWidth; i <= SimRegionWidth; ++i )
        {
            for( int j = -SimRegionWidth; j <= SimRegionWidth; ++j )
            {
                for( int k = -SimRegionWidth; k <= SimRegionWidth; ++k )
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

        for( int i = -SimRegionWidth; i <= SimRegionWidth; ++i )
        {
            for( int j = -SimRegionWidth; j <= SimRegionWidth; ++j )
            {
                for( int k = -SimRegionWidth; k <= SimRegionWidth; ++k )
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
                    v3 clusterWorldOffset = GetClusterOffsetFromOrigin( entity.stored.coords.clusterP, world->originClusterP );
                    Translate( entity.mesh->mTransform, clusterWorldOffset );
                }

                entity.state = EntityState::Active;
            }
            it.Next();
        }
    }
}

internal v3
GetLiveEntityWorldP( const UniversalCoords& coords, const v3i& worldOriginClusterP )
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


    UpdateWorldGeneration( input, world, &gameState->worldArena );




    ///// Render
    PushClear( { 0.95f, 0.95f, 0.95f, 1.0f }, renderCommands );

#if !RELEASE
    //if( !gameMemory->DEBUGglobalEditing )
#endif
    {
        PushProgramChange( ShaderProgramName::FlatShading, renderCommands );

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

        PushProgramChange( ShaderProgramName::PlainColor, renderCommands );
        PushMaterial( nullptr, renderCommands );
        u32 black = Pack01ToRGBA( 0, 0, 0, 1 );

        it = world->liveEntities.First();
        while( it )
        {
            TIMED_BLOCK;

            LiveEntity& entity = (LiveEntity&)it;
            if( entity.state == EntityState::Active )
            {
                v3 entityP = GetLiveEntityWorldP( entity.stored.coords, world->originClusterP );
                aabb entityBounds = AABB( entityP, entity.stored.dim );
                RenderBounds( entityBounds, black, renderCommands );
            }

            it.Next();
        }
#if !RELEASE
        debugState->totalEntities = world->liveEntities.count;
#endif
    }

    PushProgramChange( ShaderProgramName::PlainColor, renderCommands );

//     if( !gameMemory->DEBUGglobalEditing )
    {
        PushMaterial( world->player->mesh.material, renderCommands );
        RenderMesh( world->player->mesh, renderCommands );
    }

    // Render current cluster limits
    PushMaterial( nullptr, renderCommands );
    u32 red = Pack01ToRGBA( 1, 0, 0, 1 );
    RenderBoundsAt( V3Zero, ClusterSizeMeters , red, renderCommands );
    //RenderBoxAt( V3Zero, ClusterSizeMeters , red, renderCommands );

    Cluster* currentCluster = world->clusterTable.Find( world->originClusterP );
    r32 clusterHalfSize = ClusterSizeMeters * 0.5f;
    u32 halfRed = Pack01ToRGBA( 1, 0, 0, 0.2f );

    // Render partitioned volumes
    {
        // Volume bounds are in voxel units
        for( u32 i = 0; i < currentCluster->volumes.count; ++i )
        {
            Volume& v = currentCluster->volumes[i];
            if( v.leftChild == nullptr && v.rightChild == nullptr )
            {
                aabb worldBounds =
                {
                    v.bounds.xMin * VoxelSizeMeters - clusterHalfSize, v.bounds.xMax * VoxelSizeMeters - clusterHalfSize,
                    v.bounds.yMin * VoxelSizeMeters - clusterHalfSize, v.bounds.yMax * VoxelSizeMeters - clusterHalfSize,
                    v.bounds.zMin * VoxelSizeMeters - clusterHalfSize, v.bounds.zMax * VoxelSizeMeters - clusterHalfSize,
                };
                RenderBounds( worldBounds, halfRed, renderCommands );
            }
        }
    }

#if 0
    // Voxel grid
    v3 clusterOffsetP = -V3One * clusterHalfSize;
    v3 voxelCenterP = V3One * VoxelSizeMeters * 0.5f;
    u32 grey = Pack01ToRGBA( 0.5f, 0.5f, 0.5f, 1.f );

    for( int i = 0; i <= VoxelsPerClusterAxis; ++i )
        for( int j = 0; j <= VoxelsPerClusterAxis; ++j )
            for( int k = 0; k <= VoxelsPerClusterAxis; ++k )
            {
                if( currentCluster->voxelGrid[i][j][k] != 0 )
                {
                    v3 voxelP = V3( (r32)i, (r32)j, (r32)k ) + clusterOffsetP + voxelCenterP;
                    RenderBoxAt( voxelP, VoxelSizeMeters, grey, renderCommands );
                }
            }
#else
    RenderVoxelGrid( currentCluster->voxelGrid, VoxelSizeMeters, blue, renderCommands );
#endif

    {
        // Create a chasing camera
        // TODO Use a PID controller
        Player *player = world->player;
        v3 pCam = player->mesh.mTransform * V3( 0, -15, 7 );
        v3 pLookAt = player->mesh.mTransform * V3( 0, 1, 0 );
        v3 vUp = GetColumn( player->mesh.mTransform, 2 ).xyz;
        renderCommands->camera = DefaultCamera();
        renderCommands->camera.worldToCamera = M4CameraLookAt( pCam, pLookAt, vUp );
    }    
}
