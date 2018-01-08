void
UpdateAndRenderEditor( GameInput *input, GameMemory *memory, GameRenderCommands *renderCommands )
{
    GameState *gameState = (GameState *)memory->permanentStorage;
    EditorState &editorState = gameState->DEBUGeditorState;

    // Controls
    if( editorState.pCamera == V3.zero )
    {
        // Setup a zenithal camera initially
        editorState.pCamera = playerDude->mTransform * V3( 0, 0, 15 );
        //v3 pCamLookAt = playerDude->mTransform * V3( 0, 1, 0 );
        editorState.camYaw = 0;
        //v3 vCamUp = playerDude->mTransform * V3( 0, 1, 0 ) - gameState->pPlayer;
        editorState.camPitch = -PI / 2;
    }

    // Draw the world
    PushClear( renderCommands, { 0.95f, 0.95f, 0.95f, 1.0f } );

    float width = renderCommands->width;
    float height = renderCommands->height;

    UpdateAndRenderWorld( gameState, renderCommands );

    FlyingDude *playerDude = gameState->playerDude;
    renderCommands->mCamera = CameraLookAt( pCam, pCamLookAt, vCamUp );
}

