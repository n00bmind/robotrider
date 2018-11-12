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

#if !RELEASE

internal EditorEntity
CreateEditorEntityFor( Mesh* mesh, u32 cellsPerSide )
{
    EditorEntity result = { mesh, Pack01ToRGBA( 1, 0, 0, 1 ) };
    result.cellsPerSide = cellsPerSide;
    return result;
}

internal void
DrawEditorEntity( const EditorEntity& editorEntity, const EditorState& editorState, RenderCommands* renderCommands )
{
    if( editorEntity.mesh )
    {
        DrawBounds( editorEntity.mesh->bounds, editorEntity.color, renderCommands );

        aabb& box = editorEntity.mesh->bounds;
        r32 resolutionStep = (box.xMax - box.xMin) / editorEntity.cellsPerSide;
        //DrawCubicGrid( box, resolutionStep, Pack01ToRGBA( 1, 0, 0, 0.05f ), false, renderCommands ); 
        r32 step = resolutionStep;
        u32 color = Pack01ToRGBA( 0, 0, 0, 0.3f );
        r32 xMin = box.xMin;
        r32 xMax = box.xMax;
        r32 yMin = box.yMin;
        r32 yMax = box.yMax;
        //for( u32 layer = editorState.displayedLayer; layer <= editorState.displayedLayer + 1; ++layer )
        u32 layer = editorState.displayedLayer + 1;
        {
            r32 z = box.zMin + layer * step;

            for( r32 y = yMin; y <= yMax; y += step )
                PushLine( { xMin, y, z }, { xMax, y, z }, color, renderCommands );

            for( r32 x = xMin; x <= xMax; x += step )
            {
                PushLine( { x, yMin, z }, { x, yMax, z }, color, renderCommands );
            }
        }
    }
}

void
InitEditor( const v2i screenDim, EditorState* editorState, World* world, MemoryArena* worldArena, MemoryArena* tmpArena )
{
    // Mesh resampling test
    // TODO Move out of the way
#if 0
    editorState->testMesh = LoadOBJ( "bunny.obj", worldArena, tmpArena,
                                      Scale( V3( 10.f, 10.f, 10.f ) ) * Translation( V3( 0, 0, 1.f ) ) );

    editorState->cacheBuffers = InitMarchingCacheBuffers( worldArena, 50 );
#endif

    editorState->wfcSpecs = LoadWFCVars( "wfc.vars", worldArena, tmpArena );
    ASSERT( editorState->wfcSpecs.count );

    RandomSeed();

    /// WFC test
    if( !editorState->wfcArena.base )
        editorState->wfcArena = MakeSubArena( worldArena, MEGABYTES(512) );
    editorState->wfcState = WFC::StartWFCAsync( editorState->wfcSpecs[0], &editorState->wfcArena );

    if( !editorState->wfcDisplayArena.base )
        editorState->wfcDisplayArena = MakeSubArena( worldArena, MEGABYTES(16) );
}

void
UpdateAndRenderEditor( GameInput *input, GameMemory *memory, RenderCommands *renderCommands, const char* statsText,
                       MemoryArena* arena, MemoryArena* tmpArena )
{
    float dT = input->frameElapsedSeconds;
    float elapsedT = input->totalElapsedSeconds;

    GameState *gameState = (GameState *)memory->permanentStorage;
    EditorState &editorState = gameState->DEBUGeditorState;
    World* world = gameState->world;

    // Setup a zenithal camera initially
    if( editorState.pCamera == V3Zero )
    {
        editorState.pCamera = V3( 0, -20, 20 );
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
        editorState.pCamera += Transposed( mCamRot ) * vCamDelta;

        renderCommands->camera = DefaultCamera();
        renderCommands->camera.mTransform = mCamRot * Translation( -editorState.pCamera );
    }


    v2 displayDim = V2( renderCommands->width * 0.9f, renderCommands->height * 0.9f );
    u32 selectedIndex = WFC::DrawTest( editorState.wfcSpecs, editorState.wfcState,
                                       &editorState.wfcDisplayArena, &editorState.wfcDisplayState, displayDim );

    if( editorState.wfcState->cancellationRequested )
    {
        // Simply wait until cancelled or done
        if( editorState.wfcState->currentResult > WFC::Result::InProgress )
        {
            ClearArena( &editorState.wfcArena, true );
            ClearArena( &editorState.wfcDisplayArena, true );
            u32 savedIndex = editorState.wfcDisplayState.currentSpecIndex;
            editorState.wfcDisplayState = {};
            editorState.wfcDisplayState.currentSpecIndex = savedIndex;

            WFC::Spec& spec = editorState.wfcSpecs[editorState.wfcDisplayState.currentSpecIndex];
            editorState.wfcState = WFC::StartWFCAsync( spec, &editorState.wfcArena );
        }
    }
    else
    {
        if( selectedIndex != U32MAX )
        {
            editorState.wfcDisplayState.currentSpecIndex = selectedIndex;
            editorState.wfcState->cancellationRequested = true;
        }
    }



    // Mesh resampling test
    // TODO Move out of the way
#if 0
    if( !editorState.testIsoSurfaceMesh || input->executableReloaded )
    {
        editorState.displayedLayer = (editorState.displayedLayer + 1) % editorState.cacheBuffers.cellsPerAxis;
        editorState.lastUpdateTimeSeconds = input->totalElapsedSeconds;
        editorState.drawingDistance = Distance( GetTranslation( editorState.testMesh.mTransform ), editorState.pCamera );
        editorState.displayedLayer = 172;

        MeshPool* meshPool = &world->meshPools[0];
        if( editorState.testIsoSurfaceMesh )
            ReleaseMesh( &editorState.testIsoSurfaceMesh );
        editorState.testIsoSurfaceMesh = ConvertToIsoSurfaceMesh( editorState.testMesh, &editorState.cacheBuffers,
                                                                  meshPool, tmpArena, renderCommands, editorState );
    }

    PushProgramChange( ShaderProgramName::FlatShading, renderCommands );
    //PushMesh( editorState.testMesh, renderCommands );
    PushMesh( *editorState.testIsoSurfaceMesh, renderCommands );
#endif

    PushProgramChange( ShaderProgramName::PlainColor, renderCommands );
    PushMaterial( nullptr, renderCommands );

#if 0
	editorState.testEditorEntity = CreateEditorEntityFor(editorState.testIsoSurfaceMesh, editorState.cacheBuffers.cellsPerAxis);
	DrawEditorEntity( editorState.testEditorEntity, editorState, renderCommands );
#endif

	DrawFloorGrid(CLUSTER_HALF_SIZE_METERS * 2, gameState->world->marchingCubeSize, renderCommands);
    DrawAxisGizmos( renderCommands );

    u16 width = renderCommands->width;
    u16 height = renderCommands->height;
    r32 elapsedSeconds = input->totalElapsedSeconds;
    DrawEditorStats( width, height, statsText, (i32)elapsedSeconds % 2 == 0 );
}
#endif
