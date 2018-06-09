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

#define DATA_RELATIVE_PATH "..\\data\\"


internal u32 clusterHashFunction( const v3i& keyValue )
{
    // TODO Better hash function! x)
    u32 hashValue = (u32)(19*keyValue.x + 7*keyValue.y + 3*keyValue.z);
    return hashValue;
}

internal u32 entityHashFunction( const u32& keyValue )
{
    // TODO Better hash function! x)
    return keyValue;
}

#define INITIAL_CLUSTER_COORDS V3I( I32MAX, I32MAX, I32MAX )

void
InitWorld( World* world, MemoryArena* worldArena, MemoryArena* tmpArena )
{
#if 0
    LoadOBJ( DATA_RELATIVE_PATH "bunny.obj", &testVertices, &testIndices );
#endif

    world->player = PUSH_STRUCT( worldArena, Player );
    world->player->mesh =
        LoadOBJ( DATA_RELATIVE_PATH "feisar/feisar_ship.obj", worldArena, tmpArena );
    world->player->mesh.mTransform = M4Identity();

    world->marchingAreaSize = 10;
    world->marchingCubeSize = 1;
    srand( 1234 );

    new (&world->clusterTable) HashTable<v3i, Cluster>( worldArena, 256*1024, clusterHashFunction );
    new (&world->liveEntities) BucketArray<LiveEntity>( 256, worldArena );
    new (&world->entityRefs) HashTable<u32, StoredEntity *>( worldArena, 1024, entityHashFunction );

    world->pWorldOrigin = { 0, 0, 0 };
    world->pLastWorldOrigin = INITIAL_CLUSTER_COORDS;

    GeneratorHullNodeData* hullGenData = PUSH_STRUCT( worldArena, GeneratorHullNodeData );
    hullGenData->areaSideMeters = world->marchingAreaSize;
    hullGenData->resolutionMeters = world->marchingCubeSize;
    Generator hullGenerator = { GeneratorHullNodeFunc, &hullGenData->header };
    world->meshGenerators[0] = hullGenerator;

    world->meshPools = PUSH_ARRAY( worldArena, globalPlatform.workerThreadsCount, MarchingMeshPool );
    for( u32 i = 0; i < globalPlatform.workerThreadsCount; ++i )
    {
        // TODO Re-evaluate size of each pool now that we have many
        Init( &world->meshPools[i], world->marchingAreaSize, world->marchingCubeSize,
              worldArena, MEGABYTES(256) );
    }
}

internal void
CreateEntitiesInCluster( const v3i& clusterCoords, Cluster* cluster, Generator* meshGenerators )
{
    const r32 margin = 3.f;

    // TEST Place an entity every few meters, leaving some margin
    for( r32 x = -CLUSTER_HALF_SIZE_METERS + margin; x <= CLUSTER_HALF_SIZE_METERS - margin; x += 24.f )
    {
        for( r32 y = -CLUSTER_HALF_SIZE_METERS + margin; y <= CLUSTER_HALF_SIZE_METERS - margin; y += 24.f )
        {
            for( r32 z = -CLUSTER_HALF_SIZE_METERS + margin; z <= CLUSTER_HALF_SIZE_METERS - margin; z += 24.f )
            {
                StoredEntity newEntity =
                {
                    { clusterCoords, V3( x, y, z ) },
                    // TODO Think of how to assign specific generators to the created entities
                    &meshGenerators[0],
                };
                cluster->entityStorage.Add( newEntity );
            }
        }
    }
}

internal v3
GetClusterWorldOffset( const v3i& clusterCoords, const World* world )
{
    v3 result = V3(clusterCoords - world->pWorldOrigin) * CLUSTER_HALF_SIZE_METERS * 2;
    return result;
}

internal void
DEBUGClearAllClusters( World* world )
{
    // NOTE This very likely leaks
    world->clusterTable.Clear();
}

internal bool
IsInSimRegion( const v3i& pClusterCoords, const v3i& pWorldOrigin )
{
    v3i vRelativeCoords = pClusterCoords - pWorldOrigin;

    return vRelativeCoords.x >= -SIM_REGION_WIDTH && vRelativeCoords.x <= SIM_REGION_WIDTH &&
        vRelativeCoords.y >= -SIM_REGION_WIDTH && vRelativeCoords.y <= SIM_REGION_WIDTH &&
        vRelativeCoords.z >= -SIM_REGION_WIDTH && vRelativeCoords.z <= SIM_REGION_WIDTH;
}

inline GeneratorJob*
FindFreeJob( World* world )
{
    bool found = false;
    GeneratorJob* result = nullptr;

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
    GeneratorJob* job = (GeneratorJob*)userData;

    const v3i& clusterCoords = job->storedEntity->pUniverse.pCluster;

    if( IsInSimRegion( clusterCoords, *job->pWorldOrigin ) )
    {
        // We only have pools for worker threads and worker thread indices start at 1
        // TODO This is bad. Find a more explicity way to associated thread-job data like this
        MarchingMeshPool* meshPool = &job->meshPools[workerThreadIndex - 1];

        // Make live entity from stored and put it in the world
        *job->outputEntity =
        {
            *job->storedEntity,
            job->storedEntity->generator->func( job->storedEntity->generator->data,
                                                job->storedEntity->pUniverse,
                                                meshPool ),
            EntityState::Loaded,
        };
    }

    MEMORY_WRITE_BARRIER    

    job->occupied = false;
}

internal void
LoadEntitiesInCluster( const v3i& clusterCoords, World* world, MemoryArena* arena )
{
    TIMED_BLOCK( LoadEntitiesInCluster );

    Cluster* cluster = world->clusterTable.Find( clusterCoords );

    if( !cluster )
    {
        cluster = world->clusterTable.Reserve( clusterCoords, arena );
        cluster->populated = false;
        new (&cluster->entityStorage) BucketArray<StoredEntity>( 256, arena );
    }

    if( !cluster->populated )
    {
        CreateEntitiesInCluster( clusterCoords, cluster, world->meshGenerators );
        cluster->populated = true;
    }




    {
        TIMED_BLOCK( GenerateEntities );

        BucketArray<StoredEntity>::Idx it = cluster->entityStorage.First();
        // TODO Pre-reserve a bunch of slots and generate entities bundles and measure
        // if there's any speed difference
        while( it )
        {
            StoredEntity& storedEntity = it;
            LiveEntity* outputEntity = world->liveEntities.Reserve();
            outputEntity->state = EntityState::Invalid;

            const v3i& pWorldOrigin = world->pWorldOrigin;



    // TODO Rewrite all this as it'll need to be when paralellized
    ////////// JOB START

            GeneratorJob* job = FindFreeJob( world );
            *job =
            {
                &storedEntity,
                &world->pWorldOrigin,
                world->meshPools,
                outputEntity,
            };
            job->occupied = true;

            globalPlatform.AddNewJob( globalPlatform.hiPriorityQueue,
                                      GenerateOneEntity,
                                      job );


    ////////// JOB END



            it.Next();
        }
    }
}

internal void
StoreEntitiesInCluster( const v3i& clusterCoords, World* world, MemoryArena* arena )
{
    Cluster* cluster = world->clusterTable.Find( clusterCoords );

    if( !cluster )
    {
        // Should only happen on the very first iteration
        ASSERT( world->clusterTable.count == 0 );
        return;
    }

    cluster->entityStorage.Clear();
    v3 vClusterWorldOffset = GetClusterWorldOffset( clusterCoords, world );
    // FIXME Not nice! Better float comparisons!
    // http://floating-point-gui.de/errors/comparison/
    // https://bitbashing.io/comparing-floats.html
    // (although we may be ok for now?)
    r32 augmentedHalfSize = CLUSTER_HALF_SIZE_METERS + 0.01f;

    BucketArray<LiveEntity>::Idx it = world->liveEntities.Last();
    while( it )
    {
        LiveEntity& liveEntity = ((LiveEntity&)it);
        v3 pClusterOffset = GetTranslation( liveEntity.mesh->mTransform ) - vClusterWorldOffset;
        if( pClusterOffset.x > -augmentedHalfSize && pClusterOffset.x < augmentedHalfSize &&
            pClusterOffset.y > -augmentedHalfSize && pClusterOffset.y < augmentedHalfSize &&
            pClusterOffset.z > -augmentedHalfSize && pClusterOffset.z < augmentedHalfSize )
        {
            StoredEntity& storedEntity = liveEntity.stored;
            storedEntity.pUniverse.pCluster = clusterCoords;
            storedEntity.pUniverse.pClusterOffset = pClusterOffset;

            cluster->entityStorage.Add( storedEntity );

            ReleaseMesh( liveEntity.mesh );
            world->liveEntities.Remove( it );
        }

        it.Prev();
    }
}

internal void
UpdateWorldGeneration( GameInput* input, bool firstStepOnly, World* world, MemoryArena* arena )
{
    // TODO Make an infinite connected 'cosmic grid structure'
    // so we can test for a good cluster size, evaluate current generation speeds,
    // debug moving across clusters, etc.

    if( world->pWorldOrigin != world->pLastWorldOrigin )
    {
        v3 vWorldDelta = (world->pLastWorldOrigin - world->pWorldOrigin) * CLUSTER_HALF_SIZE_METERS * 2;

        // Offset all live entities by the world delta
        // NOTE This could be done more efficiently _after_ storing entities
        // (but we'd have to account for the delta in StoreEntitiesInCluster)
        auto it = world->liveEntities.First();
        while( it )
        {
            Translate( ((LiveEntity&)it).mesh->mTransform, vWorldDelta );
            it.Next();
        }
        // TODO Should we put the player(s) in the live entities table?
        if( world->pLastWorldOrigin != INITIAL_CLUSTER_COORDS )
            world->pPlayer += vWorldDelta;

        for( int i = -SIM_REGION_WIDTH; i <= SIM_REGION_WIDTH; ++i )
        {
            for( int j = -SIM_REGION_WIDTH; j <= SIM_REGION_WIDTH; ++j )
            {
                for( int k = -SIM_REGION_WIDTH; k <= SIM_REGION_WIDTH; ++k )
                {
                    v3i pLastClusterCoords = world->pLastWorldOrigin + V3I( i, j, k );

                    // Evict all entities contained in a cluster which is now out of bounds
                    if( !IsInSimRegion( pLastClusterCoords, world->pWorldOrigin ) )
                    {
                        // FIXME This is very dumb!
                        // We first iterate clusters and then iterate the whole liveEntities
                        // filtering by that cluster.. ¬¬ Just iterate liveEntities once and discard
                        // all which are not valid, and use the integer cluster coordinates to do so!
                        // (Just call IsInSimRegion())
                        // Also! Don't forget to check the cluster an entity belongs to
                        // everytime you move them!
                        StoreEntitiesInCluster( pLastClusterCoords, world, arena );
                    }
                }
            }
        }

        for( int i = -SIM_REGION_WIDTH; i <= SIM_REGION_WIDTH; ++i )
        {
            for( int j = -SIM_REGION_WIDTH; j <= SIM_REGION_WIDTH; ++j )
            {
                for( int k = -SIM_REGION_WIDTH; k <= SIM_REGION_WIDTH; ++k )
                {
                    v3i pClusterCoords = world->pWorldOrigin + V3I( i, j, k );

                    // Retrieve all entities contained in a cluster which is now inside bounds
                    // and put them in the live entities list
                    if( !IsInSimRegion( pClusterCoords, world->pLastWorldOrigin ) )
                    {
                        LoadEntitiesInCluster( pClusterCoords, world, arena );
                    }
                }
            }
        }
    }
    world->pLastWorldOrigin = world->pWorldOrigin;


    // Constantly monitor live entities array for inactive entities
    auto it = world->liveEntities.First();
    while( it )
    {
        LiveEntity& entity = ((LiveEntity&)it);
        if( entity.state == EntityState::Loaded )
        {
            v3 vClusterWorldOffset
                = GetClusterWorldOffset( entity.stored.pUniverse.pCluster, world );
            Translate( entity.mesh->mTransform, vClusterWorldOffset );

            entity.state = EntityState::Active;
        }
        it.Next();
    }




    // Connected paths test
#if 0
    if( !world->pathsBuffer || input->executableReloaded )
    {
        v3 vForward = V3Forward();
        v3 vUp = V3Up();
        world->pathsBuffer = Array<GeneratorPath>( arena, 1000 );
        world->pathsBuffer.Add( 
        {
            V3Zero(),
            world->marchingAreaSize,
            M4Basis( Cross( vForward, vUp ), vForward, vUp ),
            IsoSurfaceType::Cuboid,
            2,
            50, 100,
            150, 550,
            RandomRange( 50.f, 100.f ),
            RandomRange( 150.f, 550.f ),
        } );
    }

    // TODO Decide how to pack entities for archival using minimal data
    // (probably just use the GenPaths at each chunk position and regenerate)
    // TODO Implement simulation regions

    if( !world->hullMeshes )
    {
        world->hullMeshes = Array<Mesh>( arena, 10000 );

        if( firstStepOnly )
            world->hullMeshes.Place();
    }

    if( world->hullMeshes.count < 1000 ) //&& input->frameCounter % 10 == 0 )
    {
        // TODO This is all temporary
        // We need to plan this with care. Keep the buffer sorted by distance to player always
        GeneratorPath* currentPath = nullptr;
        for( u32 i = 0; i < world->pathsBuffer.count; ++i )
        {
            GeneratorPath* path = &world->pathsBuffer[i];
            if( DistanceSq( path->pCenter, world->pPlayer ) < 10000 )
            {
                currentPath = path;
                break;
            }
        }

        if( currentPath )
        {
            GeneratorPath fork;
            Mesh* outMesh = firstStepOnly ? &world->hullMeshes[0] : world->hullMeshes.Place();

            u32 numForks =
                GenerateOnePathStep( currentPath, world->marchingCubeSize, !firstStepOnly, arena, outMesh, &fork );
            if( numForks )
                world->pathsBuffer.Add( fork );
        }
    }
#endif
}

void
UpdateAndRenderWorld( GameInput *input, GameState *gameState, RenderCommands *renderCommands )
{
    float dT = input->frameElapsedSeconds;
    float elapsedT = input->totalElapsedSeconds;
    World* world = gameState->world;

    ///// Update

#if DEBUG
    if( input->executableReloaded )
    {
        DEBUGClearAllClusters( world );
        world->pLastWorldOrigin = INITIAL_CLUSTER_COORDS;
    }
#endif

    // Update player based on input
    if( input )
    {
        GameControllerInput *input0 = GetController( input, 0 );

        Player *player = world->player;
        v3 pPlayer = world->pPlayer;

        r32 playerSpeed = input0->leftThumb.endedDown ? 18.f : 10.f;
        v3 vPlayerDelta = {};

        if( input0->dLeft.endedDown )
        {
            vPlayerDelta.x -= playerSpeed * dT;
        }
        if( input0->dRight.endedDown )
        {
            vPlayerDelta.x += playerSpeed * dT;
        }
        if( input0->dUp.endedDown )
        {
            vPlayerDelta.y += playerSpeed * dT;
        }
        if( input0->dDown.endedDown )
        {
            vPlayerDelta.y -= playerSpeed * dT;
        }

        if( input0->rightStick.avgX || input0->rightStick.avgY )
        {
            world->playerPitch += -input0->rightStick.avgY / 15.f * dT;
            world->playerYaw += -input0->rightStick.avgX / 15.f * dT; 
        }

        m4 mPlayerRot = ZRotation( world->playerYaw ) * XRotation( world->playerPitch );
        pPlayer = pPlayer + mPlayerRot * vPlayerDelta;
        player->mesh.mTransform = RotPos( mPlayerRot, pPlayer )
            * Scale( V3( 0.1f, 0.1f, 0.1f ) );

        world->pPlayer = pPlayer;

        // Check if we moved to a different cluster
        {
            v3i pWorldOrigin = world->pWorldOrigin;

            if( pPlayer.x > CLUSTER_HALF_SIZE_METERS )
                pWorldOrigin.x++;
            else if( pPlayer.x < -CLUSTER_HALF_SIZE_METERS )
                pWorldOrigin.x--;
            if( pPlayer.y > CLUSTER_HALF_SIZE_METERS )
                pWorldOrigin.y++;
            else if( pPlayer.y < -CLUSTER_HALF_SIZE_METERS )
                pWorldOrigin.y--;
            if( pPlayer.z > CLUSTER_HALF_SIZE_METERS )
                pWorldOrigin.z++;
            else if( pPlayer.z < -CLUSTER_HALF_SIZE_METERS )
                pWorldOrigin.z--;

            if( pWorldOrigin != world->pWorldOrigin )
                world->pWorldOrigin = pWorldOrigin;
        }
    }


    bool firstStepOnly = false;


    UpdateWorldGeneration( input, firstStepOnly, world, &gameState->worldArena );




    ///// Render

    PushProgramChange( ShaderProgramName::FlatShaded, renderCommands );

    auto it = world->liveEntities.First();
    while( it )
    {
        LiveEntity& entity = (LiveEntity&)it;
        if( entity.state == EntityState::Active )
        {
            PushMesh( *entity.mesh, renderCommands );
        }

        it.Next();
    }

    PushMesh( world->player->mesh, renderCommands );

    PushProgramChange( ShaderProgramName::PlainColor, renderCommands );

    // Render current cluster limits
    r32 s = CLUSTER_HALF_SIZE_METERS;
    u32 red = Pack01ToRGBA( V4( 1, 0, 0, 1 ) );

    // X
    PushLine( V3( -s, s, s ), V3( s, s, s ), red, renderCommands );
    PushLine( V3( -s, s, -s ), V3( s, s, -s ), red, renderCommands );
    PushLine( V3( -s, -s, s ), V3( s, -s, s ), red, renderCommands );
    PushLine( V3( -s, -s, -s ), V3( s, -s, -s ), red, renderCommands );
    // Z
    PushLine( V3( -s, s, s ), V3( -s, s, -s ), red, renderCommands );
    PushLine( V3( s, s, s ), V3( s, s, -s ), red, renderCommands );
    PushLine( V3( -s, -s, s ), V3( -s, -s, -s ), red, renderCommands );
    PushLine( V3( s, -s, s ), V3( s, -s, -s ), red, renderCommands );
    // Y
    PushLine( V3( -s, -s, s ), V3( -s, s, s ), red, renderCommands );
    PushLine( V3( -s, -s, -s ), V3( -s, s, -s ), red, renderCommands );
    PushLine( V3( s, -s, s ), V3( s, s, s ), red, renderCommands );
    PushLine( V3( s, -s, -s ), V3( s, s, -s ), red, renderCommands );

    {
        // Create a chasing camera
        // TODO Use a PID controller
        Player *player = world->player;
        v3 pCam = player->mesh.mTransform * V3( 0, -2, 1 );
        v3 pLookAt = player->mesh.mTransform * V3( 0, 1, 0 );
        v3 vUp = GetColumn( player->mesh.mTransform, 2 ).xyz;
        renderCommands->camera.mTransform = CameraLookAt( pCam, pLookAt, vUp );
    }
    
    // Mesh simplification test
#if 0
    Mesh testMesh;
    {
        if( ((i32)input->frameCounter - 180) % 300 == 0 )
        {
            InflatedMesh gen;
            {
                genVertices.count = testVertices.count;
                for( u32 i = 0; i < genVertices.count; ++i )
                    genVertices[i].p = testVertices[i].p;
                genTriangles.count = testIndices.count / 3;
                for( u32 i = 0; i < genTriangles.count; ++i )
                {
                    genTriangles[i].v[0] = testIndices[i*3];
                    genTriangles[i].v[1] = testIndices[i*3 + 1];
                    genTriangles[i].v[2] = testIndices[i*3 + 2];
                }
                gen.vertices = genVertices;
                gen.triangles = genTriangles;
            }
            FastQuadricSimplify( &gen, (u32)(gen.triangles.count * 0.75f) );

            testVertices.count = gen.vertices.count;
            for( u32 i = 0; i < testVertices.count; ++i )
                testVertices[i].p = gen.vertices[i].p;
            testIndices.count = gen.triangles.count * 3;
            for( u32 i = 0; i < gen.triangles.count; ++i )
            {
                testIndices[i*3 + 0] = gen.triangles[i].v[0];
                testIndices[i*3 + 1] = gen.triangles[i].v[1];
                testIndices[i*3 + 2] = gen.triangles[i].v[2];
            }
        }

        testMesh.vertices = testVertices.data;
        testMesh.indices = testIndices.data;
        testMesh.vertexCount = testVertices.count;
        testMesh.indexCount = testIndices.count;
        testMesh.mTransform = Scale({ 10, 10, 10 });
    }
    PushMesh( testMesh, renderCommands );
#endif
}
