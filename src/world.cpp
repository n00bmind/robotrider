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

        cube.mTransform = M4Translation( { transX, transY, -1.0f } );
    }
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

    PushRenderGroup( gameState->playerDude, renderCommands);

    for( u32 i = 0; i < gameState->cubeCount; ++i )
    {
        CubeThing *cube = gameState->cubes + i;
        PushRenderGroup( cube, renderCommands);
    }
}
