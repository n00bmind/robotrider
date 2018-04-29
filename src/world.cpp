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


#if 0
// FIXME Put this in some arena
internal SArray<TexturedVertex, 65536> testVertices;
internal SArray<u32, 256*1024> testIndices;
// FIXME
internal SArray<Vertex, 65536> genVertices;
internal SArray<Triangle, 256*1024> genTriangles;
#endif

internal u32 clusterHashFunction( const v3i& keyValue )
{
    // TODO Better hash function! x)
    u32 hashValue = (u32)(19*keyValue.x + 7*keyValue.y + 3*keyValue.z);
    return hashValue;
}

void
InitWorld( World* world, MemoryArena* worldArena )
{
    world->playerDude = PUSH_STRUCT( worldArena, FlyingDude );
    FlyingDude &playerDude = *world->playerDude;
    playerDude =
    {
        {
            { -0.3f,   -0.5f,  0.0f, },
            {  0.3f,   -0.5f,  0.0f, },
            {  0.0f,    1.0f,  0.0f, },
            //{  0.0f,     0.5f	 0.5f, },
        },
        {
            0, 1, 2,
            //2, 1, 3,
            //2, 3, 0,
            //3, 1, 0,
        },
    };
    playerDude.mTransform = M4Identity();

#if 0
    LoadOBJ( DATA_RELATIVE_PATH "bunny.obj", &testVertices, &testIndices );
#endif

    world->marchingAreaSize = 10;
    world->marchingCubeSize = 1;
    srand( 1234 );

    world->clusterTable = HashTable<v3i, Cluster>( worldArena, 256*1024, clusterHashFunction );

    world->pWorldOrigin = { 0, 0, 0 };
    world->pLastWorldOrigin = { I32MAX, I32MAX, I32MAX };
}

internal void
CreateEntitiesInCluster( const v3i& clusterCoords, Cluster* cluster, World* world, MemoryArena* arena )
{
    const r32 margin = 5.f;

    // TEST Place an entity every 5 meters, leaving some margin
    for( r32 i = -CLUSTER_HALF_SIZE_METERS + margin; i <= CLUSTER_HALF_SIZE_METERS - margin; i += 5.f )
    {
        for( r32 j = -CLUSTER_HALF_SIZE_METERS + margin; j <= CLUSTER_HALF_SIZE_METERS - margin; j += 5.f )
        {
            for( r32 k = -CLUSTER_HALF_SIZE_METERS + margin; k <= CLUSTER_HALF_SIZE_METERS - margin; k += 5.f )
            {
                cluster->entityStorage.Add(
                {
                    clusterCoords,
                    V3( i, j, k ),
                    (Generator*)&world->hullNodeGenerator,
                }, arena );
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
LoadEntitiesInCluster( const v3i& clusterCoords, World* world, MemoryArena* arena )
{
    Cluster* cluster = world->clusterTable.Find( clusterCoords );

    if( !cluster )
    {
        Cluster newCluster = { false, BucketArray<StoredEntity>( 256, arena ) };
        world->clusterTable.Add( clusterCoords, newCluster, arena );
    }

    if( !cluster->populated )
    {
        CreateEntitiesInCluster( clusterCoords, cluster, world, arena );
        cluster->populated = true;
    }

    v3 vClusterWorldOffset = GetClusterWorldOffset( clusterCoords, world );

    BucketArray<StoredEntity>::Idx it = cluster->entityStorage.First();
    while( it )
    {
        StoredEntity& storedEntity = it;

        // Make live entity from stored and put it in the world
        LiveEntity liveEntity = 
        {
            storedEntity,
            vClusterWorldOffset + storedEntity.pClusterOffset,
            storedEntity.generator->func( storedEntity.generator ),
        };
        world->liveEntities.Add( liveEntity, arena );

        it.Next();
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

    BucketArray<LiveEntity>::Idx it = world->liveEntities.Last();
    while( it )
    {
        LiveEntity& liveEntity = ((LiveEntity&)it);
        v3 pClusterOffset = liveEntity.p - vClusterWorldOffset;
        if( pClusterOffset.x > -CLUSTER_HALF_SIZE_METERS && pClusterOffset.x < CLUSTER_HALF_SIZE_METERS &&
            pClusterOffset.y > -CLUSTER_HALF_SIZE_METERS && pClusterOffset.y < CLUSTER_HALF_SIZE_METERS &&
            pClusterOffset.z > -CLUSTER_HALF_SIZE_METERS && pClusterOffset.z < CLUSTER_HALF_SIZE_METERS )
        {
            StoredEntity& storedEntity = liveEntity.stored;
            storedEntity.pCluster = clusterCoords;
            storedEntity.pClusterOffset = pClusterOffset;

            cluster->entityStorage.Add( storedEntity, arena );
            world->liveEntities.Remove( it );
        }

        it.Prev();
    }
}

internal bool
IsInSimApron( const v3i& pClusterCoords, const v3i& pWorldOrigin )
{
    v3i vRelativeCoords = pClusterCoords - pWorldOrigin;

    return vRelativeCoords.x >= -SIM_APRON_WIDTH && vRelativeCoords.x <= SIM_APRON_WIDTH &&
        vRelativeCoords.y >= -SIM_APRON_WIDTH && vRelativeCoords.y <= SIM_APRON_WIDTH &&
        vRelativeCoords.z >= -SIM_APRON_WIDTH && vRelativeCoords.z <= SIM_APRON_WIDTH;
}

internal void
UpdateWorldGeneration( GameInput* input, bool firstStepOnly, World* world, MemoryArena* arena )
{
    // TODO Make an infinite connected 'cosmic grid structure'
    // so we can test for a good cluster size, evaluate current generation speeds,
    // debug moving across clusters, etc.

    if( world->pWorldOrigin != world->pLastWorldOrigin )
    {
        for( int i = -SIM_APRON_WIDTH; i <= SIM_APRON_WIDTH; ++i )
        {
            for( int j = -SIM_APRON_WIDTH; j <= SIM_APRON_WIDTH; ++j )
            {
                for( int k = -SIM_APRON_WIDTH; k <= SIM_APRON_WIDTH; ++k )
                {
                    v3i pLastClusterCoords = world->pLastWorldOrigin + V3I( i, j, k );

                    // Evict all entities contained in a cluster which is now out of bounds
                    if( !IsInSimApron( pLastClusterCoords, world->pWorldOrigin ) )
                    {
                        StoreEntitiesInCluster( pLastClusterCoords, world, arena );
                    }
                }
            }
        }

        for( int i = -SIM_APRON_WIDTH; i <= SIM_APRON_WIDTH; ++i )
        {
            for( int j = -SIM_APRON_WIDTH; j <= SIM_APRON_WIDTH; ++j )
            {
                for( int k = -SIM_APRON_WIDTH; k <= SIM_APRON_WIDTH; ++k )
                {
                    v3i pClusterCoords = world->pWorldOrigin + V3I( i, j, k );

                    // Retrieve all entities contained in a cluster which is now inside bounds
                    // and put them in the live entities list
                    if( !IsInSimApron( pClusterCoords, world->pLastWorldOrigin ) )
                    {
                        LoadEntitiesInCluster( pClusterCoords, world, arena );
                    }
                }
            }
        }
    }

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
UpdateAndRenderWorld( GameInput *input, GameState *gameState, GameRenderCommands *renderCommands )
{
    float dT = input->frameElapsedSeconds;
    float elapsedT = input->totalElapsedSeconds;
    World* world = gameState->world;

    ///// Update

#if DEBUG
    // Update player based on input
    if( !gameState->DEBUGglobalEditing )
#endif
    {
        GameControllerInput *input0 = GetController( input, 0 );

        FlyingDude *playerDude = world->playerDude;
        v3 pPlayer = world->pPlayer;
        r32 playerSpeed = 9.f;
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
        playerDude->mTransform = RotPos( mPlayerRot, pPlayer );

        world->pPlayer = pPlayer;
    }


    bool firstStepOnly = false;


    UpdateWorldGeneration( input, firstStepOnly, world, &gameState->worldArena );

    ///// Render

    for( u32 i = 0; i < world->hullMeshes.count; ++i )
    {
        PushMesh( world->hullMeshes[i], renderCommands );
        if( firstStepOnly )
            break;
    }

    PushRenderGroup( world->playerDude, renderCommands);

    {
        FlyingDude *playerDude = world->playerDude;
        // Create a chasing camera
        // TODO Use a PID controller
        v3 pCam = playerDude->mTransform * V3( 0, -2, 1 );
        v3 pLookAt = playerDude->mTransform * V3( 0, 1, 0 );
        v3 vUp = GetColumn( playerDude->mTransform, 2 ).xyz;
        renderCommands->camera.mTransform = CameraLookAt( pCam, pLookAt, vUp );
    }
    
    // Mesh simplification test
#if 0
    Mesh testMesh;
    {
        if( ((i32)input->frameCounter - 180) % 300 == 0 )
        {
            GeneratedMesh gen;
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
