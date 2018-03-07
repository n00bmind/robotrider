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


void
InitWorld( GameState *gameState )
{
    gameState->playerDude = PUSH_STRUCT( &gameState->gameArena, FlyingDude );
    FlyingDude &playerDude = *gameState->playerDude;
    playerDude =
    {
        {
            {  0.5f,   -0.5f,  0.0f, },
            { -0.5f,   -0.5f,  0.0f, },
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
    gameState->cubeCount = rowSize * rowSize;
    gameState->cubes = PUSH_ARRAY( &gameState->gameArena, gameState->cubeCount, CubeThing );

    for( u32 i = 0; i < gameState->cubeCount; ++i )
    {
        CubeThing &cube = gameState->cubes[i];
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

    LoadOBJ( DATA_RELATIVE_PATH "bunny.obj", &testVertices, &testIndices );
}

internal void
UpdateWorldGeneration( GameState *gameState )
{
    // To make things simple, we're gonna start only in 2D

    // If we have no entries in the queue, pick up a random position based on player position
}

void
UpdateAndRenderWorld( GameState *gameState, GameRenderCommands *renderCommands )
{
    ///// Update

    UpdateWorldGeneration( gameState );

    ///// Render

    Mesh testMesh;
    testMesh.vertices = testVertices.data;
    testMesh.indices = testIndices.data;
    testMesh.vertexCount = testVertices.count;
    testMesh.indexCount = testIndices.count;
    testMesh.mTransform = Scale({ 10, 10, 10 });
    PushMesh( testMesh, renderCommands );


    PushRenderGroup( gameState->playerDude, renderCommands);

    for( u32 i = 0; i < gameState->cubeCount; ++i )
    {
        CubeThing *cube = gameState->cubes + i;
        //PushRenderGroup( cube, renderCommands);
    }
}
