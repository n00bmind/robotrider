void
UpdateAndRenderEditor( GameInput *input, GameMemory *memory, GameRenderCommands *renderCommands )
{
    GameState *gameState = (GameState *)memory->permanentStorage;
    EditorState &editorState = gameState->DEBUGeditorState;

    FlyingDude *playerDude = gameState->playerDude;
    
    if( editorState.pCamera == V3Zero() )
    {
        // Setup a zenithal camera initially
        editorState.pCamera = V3( 0, -5, 10 );
        editorState.camYaw = 0;
        editorState.camPitch = 0;
        //v3 pLookAt = gameState->pPlayer;
        //m4 mInitialRot = CameraLookAt( editorState.pCamera, pLookAt, V3Up() );
        editorState.camPitch = -PI32 / 4.f;
    }

    // Update camera based on input
    {
        float dt = input->frameElapsedSeconds;
        GameControllerInput *input0 = GetController( input, 0 );

        v3 vCamDelta = {};
        if( input0->dLeft.endedDown )
            vCamDelta.x -= 3.f * dt;
        if( input0->dRight.endedDown )
            vCamDelta.x += 3.f * dt;

        if( input0->dUp.endedDown )
            vCamDelta.z -= 3.f * dt;
        if( input0->dDown.endedDown )
            vCamDelta.z += 3.f * dt;

        if( input0->leftShoulder.endedDown )
            vCamDelta.y -= 3.f * dt;
        if( input0->rightShoulder.endedDown )
            vCamDelta.y += 3.f * dt;

        if( input0->rightStick.avgX || input0->rightStick.avgY )
        {
            editorState.camPitch += input0->rightStick.avgY / 15.f * dt;
            editorState.camYaw += input0->rightStick.avgX / 15.f * dt; 
        }

        m4 mCamRot = XRotation( editorState.camPitch )
            * ZRotation( editorState.camYaw );

        v3 vCamForward = GetColumn( mCamRot, 2 ).xyz;
        v3 vCamRight = GetColumn( mCamRot, 0 ).xyz;
        //editorState.pCamera += vCamForward * vCamDelta.z + vCamRight * vCamDelta.x;
        editorState.pCamera += Transposed( mCamRot ) * vCamDelta;

        renderCommands->camera.mTransform = mCamRot * M4Translation( -editorState.pCamera );
    }

    // Draw the world
    PushClear( renderCommands, { 0.95f, 0.95f, 1.0f, 1.0f } );

    u16 width = renderCommands->width;
    u16 height = renderCommands->height;

    UpdateAndRenderWorld( gameState, renderCommands );
    r32 elapsedSeconds = input->gameElapsedSeconds;
    DrawEditorNotice( width, height, (i32)elapsedSeconds % 2 == 0 );

    DrawAxisGizmos( renderCommands );
}

