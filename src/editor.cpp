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

void
UpdateAndRenderEditor( GameInput *input, GameMemory *memory, GameRenderCommands *renderCommands )
{
    GameState *gameState = (GameState *)memory->permanentStorage;
    EditorState &editorState = gameState->DEBUGeditorState;

    FlyingDude *playerDude = gameState->playerDude;
    
    // Setup a zenithal camera initially
    if( editorState.pCamera == V3Zero() )
    {
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

    u16 width = renderCommands->width;
    u16 height = renderCommands->height;

    // Draw the world
    {
        PushClear( { 0.95f, 0.95f, 1.0f, 1.0f }, renderCommands );

        UpdateAndRenderWorld( gameState, renderCommands );
    }

    // Draw marching cubes tests
    {
        const r32 TEST_MARCHED_AREA_SIZE = 10;
        const r32 TEST_MARCHED_CUBE_SIZE = 1;

        const r32 AHALF = TEST_MARCHED_AREA_SIZE / 2;
        r32 zStart = -AHALF;
        r32 zEnd = AHALF;

        u32 black = Pack01ToRGBA( V4( 0, 0, 0, 1 ) );
        v3 off = V3( 0, -AHALF, AHALF );

        for( float x = -AHALF; x < AHALF; x += TEST_MARCHED_CUBE_SIZE )
        {
            for( float y = -AHALF; y < AHALF; y += TEST_MARCHED_CUBE_SIZE )
            {
               PushLine( V3( x, y, zStart ) + off, V3( x, y, zEnd ) + off, black, renderCommands );
            }
        }
    }

    // FIXME Not drawn when issued after the lines!
    DrawAxisGizmos( renderCommands );

    r32 elapsedSeconds = input->gameElapsedSeconds;
    DrawEditorNotice( width, height, (i32)elapsedSeconds % 2 == 0 );

}

