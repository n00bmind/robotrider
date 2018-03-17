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


// FIXME Put this in some arena
internal SArray<TexturedVertex, 65536> testVertices;
internal SArray<u32, 256*1024> testIndices;
// FIXME
internal SArray<Vertex, 65536> genVertices;
internal SArray<Triangle, 256*1024> genTriangles;


void
InitWorld( World* world, MemoryArena* worldArena )
{
    world->playerDude = PUSH_STRUCT( worldArena, FlyingDude );
    FlyingDude &playerDude = *world->playerDude;
    playerDude =
    {
        {
            { -0.5f,   -0.5f,  0.0f, },
            {  0.5f,   -0.5f,  0.0f, },
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

    int rowSize = 31;
    int rowHalf = rowSize >> 1;
    world->cubeCount = rowSize * rowSize;
    world->cubes = PUSH_ARRAY( worldArena, world->cubeCount, CubeThing );

    for( u32 i = 0; i < world->cubeCount; ++i )
    {
        CubeThing &cube = world->cubes[i];
        cube =
        {
            {
                { -0.5f,    -0.5f,      0.0f },
                { -0.5f,     0.5f,      0.0f },
                {  0.5f,    -0.5f,      0.0f },
                {  0.5f,     0.5f,      0.0f },
            },
            {
                0, 1, 2,
                2, 1, 3
            },
        };

        r32 transX = (((i32)i % rowSize) - rowHalf) * 2.0f;
        r32 transY = ((i32)i / rowSize) * 2.0f;

        cube.mTransform = Translation( { transX, transY, -1.0f } );
    }

#if 0
    LoadOBJ( DATA_RELATIVE_PATH "bunny.obj", &testVertices, &testIndices );
#endif
}

internal void
UpdateWorldGeneration( World* world )
{
    // If we have no entries in the queue, pick up a random position based on player position
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
        v3 vPlayerDelta = {};

        if( input0->dLeft.endedDown )
        {
            vPlayerDelta.x -= 3.f * dT;
        }
        if( input0->dRight.endedDown )
        {
            vPlayerDelta.x += 3.f * dT;
        }
        if( input0->dUp.endedDown )
        {
            vPlayerDelta.y += 3.f * dT;
        }
        if( input0->dDown.endedDown )
        {
            vPlayerDelta.y -= 3.f * dT;
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

    UpdateWorldGeneration( world );

    ///// Render

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

    PushRenderGroup( world->playerDude, renderCommands);

    for( u32 i = 0; i < world->cubeCount; ++i )
    {
        CubeThing *cube = world->cubes + i;
        PushRenderGroup( cube, renderCommands);
    }

    {
        FlyingDude *playerDude = world->playerDude;
        // Create a chasing camera
        // TODO Use a PID controller
        v3 pCam = playerDude->mTransform * V3( 0, -2, 1 );
        v3 pLookAt = playerDude->mTransform * V3( 0, 1, 0 );
        v3 vUp = GetColumn( playerDude->mTransform, 2 ).xyz;
        renderCommands->camera.mTransform = CameraLookAt( pCam, pLookAt, vUp );
    }
}
