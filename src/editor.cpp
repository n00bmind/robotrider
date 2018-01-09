void
UpdateAndRenderEditor( GameInput *input, GameMemory *memory, GameRenderCommands *renderCommands )
{
    GameState *gameState = (GameState *)memory->permanentStorage;
    EditorState &editorState = gameState->DEBUGeditorState;

    FlyingDude *playerDude = gameState->playerDude;
    
    if( editorState.pCamera == V3Zero() )
    {
        // Setup a zenithal camera initially
        editorState.pCamera = playerDude->mTransform * V3( 0, 0, 15 );
        //v3 pCamLookAt = playerDude->mTransform * V3( 0, 1, 0 );
        editorState.camYaw = 0;
        //v3 vCamUp = playerDude->mTransform * V3( 0, 1, 0 ) - gameState->pPlayer;
        editorState.camPitch = -PI32 / 2;
    }

    // Update camera based on input
    {
        float dt = input->frameElapsedSeconds;
        GameControllerInput *input0 = GetController( input, 0 );

        v3 pCam = editorState.pCamera;
        v3 vCamDelta = {};

        //if( input0->dLeft.endedDown )
        //{
            //vCamDelta.x -= 3.f * dt;
        //}
        //if( input0->dRight.endedDown )
        //{
            //vCamDelta.x += 3.f * dt;
        //}
        //if( input0->dUp.endedDown )
        //{
            //vCamDelta.y += 3.f * dt;
        //}
        //if( input0->dDown.endedDown )
        //{
            //vCamDelta.y -= 3.f * dt;
        //}

        //if( input0->rightStick.avgX || input0->rightStick.avgY )
        //{
            //editorState.camPitch += -input0->rightStick.avgY / 15.f * dt;
            //editorState.camYaw += -input0->rightStick.avgX / 15.f * dt; 
        //}

        m4 mCamRot = ZRotation( editorState.camYaw ) * XRotation( editorState.camPitch );
        pCam = pCam + mCamRot * vCamDelta;
        // FIXME Review how camera matrices are built
        renderCommands->mCamera = RotPos( Transposed( mCamRot ), pCam );

        editorState.pCamera = pCam;
    }

    // Draw the world
    PushClear( renderCommands, { 0.95f, 0.95f, 0.95f, 1.0f } );

    float width = renderCommands->width;
    float height = renderCommands->height;

    UpdateAndRenderWorld( gameState, renderCommands );

    // TODO Draw gizmos for world axes
}

