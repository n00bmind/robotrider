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
DrawTestGrid( r32 areaSizeMeters, r32 resolutionMeters, GameRenderCommands* renderCommands )
{
    const r32 areaHalf = areaSizeMeters / 2;

    u32 semiBlack = Pack01ToRGBA( V4( 0, 0, 0, 0.1f ) );
    v3 off = V3Zero();

    r32 zStart = -areaHalf;
    r32 zEnd = areaHalf;
    for( float x = -areaHalf; x <= areaHalf; x += resolutionMeters )
    {
        for( float y = -areaHalf; y <= areaHalf; y += resolutionMeters )
        {
            PushLine( V3( x, y, zStart ) + off, V3( x, y, zEnd ) + off, semiBlack, renderCommands );
        }
    }
    r32 yStart = -areaHalf;
    r32 yEnd = areaHalf;
    for( float x = -areaHalf; x <= areaHalf; x += resolutionMeters )
    {
        for( float z = -areaHalf; z <= areaHalf; z += resolutionMeters )
        {
            PushLine( V3( x, yStart, z ) + off, V3( x, yEnd, z ) + off, semiBlack, renderCommands );
        }
    }
    r32 xStart = -areaHalf;
    r32 xEnd = areaHalf;
    for( float z = -areaHalf; z <= areaHalf; z += resolutionMeters )
    {
        for( float y = -areaHalf; y <= areaHalf; y += resolutionMeters )
        {
            PushLine( V3( xStart, y, z ) + off, V3( xEnd, y, z ) + off, semiBlack, renderCommands );
        }
    }

}
#if DEBUG
void
UpdateAndRenderEditor( GameInput *input, GameMemory *memory, GameRenderCommands *renderCommands, const char* statsText )
{
    float dT = input->frameElapsedSeconds;
    float elapsedT = input->totalElapsedSeconds;

    GameState *gameState = (GameState *)memory->permanentStorage;
    EditorState &editorState = gameState->DEBUGeditorState;

    // Setup a zenithal camera initially
    if( editorState.pCamera == V3Zero() )
    {
        editorState.pCamera = V3( 0, -12, 12 );
        editorState.camYaw = 0;
        editorState.camPitch = 0;
        //v3 pLookAt = gameState->pPlayer;
        //m4 mInitialRot = CameraLookAt( editorState.pCamera, pLookAt, V3Up() );
        editorState.camPitch = -PI32 / 4.f;
    }

    // Update camera based on input
    {
        GameControllerInput *input0 = GetController( input, 0 );

        v3 vCamDelta = {};
        if( input0->dLeft.endedDown )
            vCamDelta.x -= 3.f * dT;
        if( input0->dRight.endedDown )
            vCamDelta.x += 3.f * dT;

        if( input0->dUp.endedDown )
            vCamDelta.z -= 3.f * dT;
        if( input0->dDown.endedDown )
            vCamDelta.z += 3.f * dT;

        if( input0->leftShoulder.endedDown )
            vCamDelta.y -= 3.f * dT;
        if( input0->rightShoulder.endedDown )
            vCamDelta.y += 3.f * dT;

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

    const r32 TEST_MARCHED_AREA_SIZE = 10;
    const r32 TEST_MARCHED_CUBE_SIZE = 1;

    // Draw marching cubes tests
    v3 vForward = V3Forward();
    v3 vUp = V3Up();
    local_persistent GenPath testPath =
    {
        V3Zero(),
        TEST_MARCHED_AREA_SIZE,
        M4Basis( Cross( vForward, vUp ), vForward, vUp ),
        IsoSurfaceType::Cuboid,
        2,
        RandomRange( 0.f, 100.f ), RandomRange( 0.f, 100.f )
    };

    // FIXME
    local_persistent Mesh testMeshes[1000];
    local_persistent u32 testMeshCount = 0;

    if( input->frameCounter % 300 == 0 )
    {
        ASSERT( testMeshCount < ARRAYCOUNT(testMeshes) );
        testMeshes[testMeshCount++] = GenerateOnePathStep( &testPath, TEST_MARCHED_CUBE_SIZE );
    }
    DrawTestGrid( TEST_MARCHED_AREA_SIZE, TEST_MARCHED_CUBE_SIZE, renderCommands );
    //for( u32 i = 0; i < testMeshCount; ++i )
        //PushMesh( testMeshes[i], renderCommands );

    // FIXME Draw this as plain color always
    DrawAxisGizmos( renderCommands );

    r32 elapsedSeconds = input->totalElapsedSeconds;
    DrawEditorStats( width, height, statsText, (i32)elapsedSeconds % 2 == 0 );

}
#endif
