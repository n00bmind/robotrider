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

internal void
DrawFloorGrid( r32 areaSizeMeters, r32 resolutionMeters, RenderCommands* renderCommands )
{
    const r32 areaHalf = areaSizeMeters / 2;

    u32 semiBlack = Pack01ToRGBA( V4( 0, 0, 0, 0.1f ) );
    v3 off = V3Zero();

    r32 yStart = -areaHalf;
    r32 yEnd = areaHalf;
    for( float x = -areaHalf; x <= areaHalf; x += resolutionMeters )
    {
        PushLine( V3( x, yStart, 0 ) + off, V3( x, yEnd, 0 ) + off, semiBlack, renderCommands );
    }
    r32 xStart = -areaHalf;
    r32 xEnd = areaHalf;
    for( float y = -areaHalf; y <= areaHalf; y += resolutionMeters )
    {
        PushLine( V3( xStart, y, 0 ) + off, V3( xEnd, y, 0 ) + off, semiBlack, renderCommands );
    }
}

#if DEBUG
void
UpdateAndRenderEditor( GameInput *input, GameMemory *memory, RenderCommands *renderCommands, const char* statsText )
{
    float dT = input->frameElapsedSeconds;
    float elapsedT = input->totalElapsedSeconds;

    GameState *gameState = (GameState *)memory->permanentStorage;
    EditorState &editorState = gameState->DEBUGeditorState;

    // Setup a zenithal camera initially
    if( editorState.pCamera == V3Zero() )
    {
        editorState.pCamera = V3( 0, -50, 50 );
        editorState.camYaw = 0;
        editorState.camPitch = 0;
        //v3 pLookAt = gameState->pPlayer;
        //m4 mInitialRot = CameraLookAt( editorState.pCamera, pLookAt, V3Up() );
        editorState.camPitch = -PI / 4.f;
    }

    // Update camera based on input
    {
        GameControllerInput *input0 = GetController( input, 0 );
        r32 camSpeed = 9.f;

        v3 vCamDelta = {};
        if( input0->dLeft.endedDown )
            vCamDelta.x -= camSpeed * dT;
        if( input0->dRight.endedDown )
            vCamDelta.x += camSpeed * dT;

        if( input0->dUp.endedDown )
            vCamDelta.z -= camSpeed * dT;
        if( input0->dDown.endedDown )
            vCamDelta.z += camSpeed * dT;

        if( input0->leftShoulder.endedDown )
            vCamDelta.y -= camSpeed * dT;
        if( input0->rightShoulder.endedDown )
            vCamDelta.y += camSpeed * dT;

        if( input0->rightStick.avgX || input0->rightStick.avgY )
        {
            editorState.camPitch += input0->rightStick.avgY / 15.f * dT;
            editorState.camYaw += input0->rightStick.avgX / 15.f * dT; 
        }

        m4 mCamRot = XRotation( editorState.camPitch )
            * ZRotation( editorState.camYaw );

        v3 vCamForward = GetColumn( mCamRot, 2 ).xyz;
        v3 vCamRight = GetColumn( mCamRot, 0 ).xyz;
        //editorState.pCamera += vCamForward * vCamDelta.z + vCamRight * vCamDelta.x;
        editorState.pCamera += Transposed( mCamRot ) * vCamDelta;

        renderCommands->camera.mTransform = mCamRot * Translation( -editorState.pCamera );
    }

    u16 width = renderCommands->width;
    u16 height = renderCommands->height;

    PushProgramChange( ShaderProgramName::PlainColor, renderCommands );

    DrawFloorGrid( CLUSTER_HALF_SIZE_METERS * 2, gameState->world->marchingCubeSize, renderCommands );

    // FIXME Draw this as plain color always
    DrawAxisGizmos( renderCommands );

    r32 elapsedSeconds = input->totalElapsedSeconds;
    DrawEditorStats( width, height, statsText, (i32)elapsedSeconds % 2 == 0 );

}
#endif
