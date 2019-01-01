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
InitEditor( const v2i screenDim, GameState* gameState, EditorState* editorState, TransientState* transientState,
            MemoryArena* editorArena, MemoryArena* transientArena )
{
    // Mesh resampling test
    // TODO Move out of the way
#if 0
    editorState->testMesh = LoadOBJ( "bunny.obj", worldArena, tmpMemory,
                                      Scale( V3( 10.f, 10.f, 10.f ) ) * Translation( V3( 0, 0, 1.f ) ) );

    editorState->cacheBuffers = InitMarchingCacheBuffers( worldArena, 50 );
#endif

    RandomSeed();

    /// WFC test
    if( !IsInitialized( transientState->wfcArena ) )
        transientState->wfcArena = MakeSubArena( transientArena, MEGABYTES(512) );
    if( !IsInitialized( transientState->wfcDisplayArena ) )
        transientState->wfcDisplayArena = MakeSubArena( transientArena, MEGABYTES(16) );

    TemporaryMemory tempMemory = BeginTemporaryMemory( transientArena );

    transientState->wfcSpecs = LoadWFCVars( "wfc.vars", editorArena, tempMemory );
    ASSERT( transientState->wfcSpecs.count );

    EndTemporaryMemory( tempMemory );
}

void
UpdateAndRenderEditor( GameInput *input, GameState* gameState, TransientState* transientState, 
                       DebugState* debugState, RenderCommands *renderCommands, const char* statsText, 
                       const TemporaryMemory& frameMemory )
{
    float dT = input->frameElapsedSeconds;
    float elapsedT = input->totalElapsedSeconds;

    EditorState* editorState = &gameState->DEBUGeditorState;
    World* world = gameState->world;

    // Setup a zenithal camera initially
    if( editorState->pCamera == V3Zero )
    {
        editorState->pCamera = V3( 0, -20, 20 );
        editorState->camYaw = 0;
        editorState->camPitch = 0;
        //v3 pLookAt = gameState->pPlayer;
        //m4 mInitialRot = CameraLookAt( editorState->pCamera, pLookAt, V3Up() );
        editorState->camPitch = -PI / 4.f;
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
            editorState->camPitch += input0->rightStick.avgY / 15.f * dT;
            editorState->camYaw += input0->rightStick.avgX / 15.f * dT; 
        }

        m4 mCamRot = XRotation( editorState->camPitch )
            * ZRotation( editorState->camYaw );

        v3 vCamForward = GetColumn( mCamRot, 2 ).xyz;
        v3 vCamRight = GetColumn( mCamRot, 0 ).xyz;
        editorState->pCamera += Transposed( mCamRot ) * vCamDelta;

        renderCommands->camera = DefaultCamera();
        renderCommands->camera.mTransform = mCamRot * Translation( -editorState->pCamera );
    }


    if( !transientState->wfcGlobalState )
    {
        WFC::Spec& spec = transientState->wfcSpecs[transientState->wfcDisplayState.currentSpecIndex];
        WFC::Spec defaultSpec = WFC::DefaultSpec();
        // Ignore certain attributes
        spec.outputChunkDim = defaultSpec.outputChunkDim;
        spec.periodic = defaultSpec.periodic;

        transientState->wfcGlobalState = WFC::StartWFCAsync( spec, { 0, 0 }, &transientState->wfcArena );
    }

    WFC::UpdateWFCAsync( transientState->wfcGlobalState );
    {
        v2 renderDim = V2( renderCommands->width, renderCommands->height );
        v2 displayDim = renderDim * 0.9f;
        v2 pDisplay = (renderDim - displayDim) / 2.f;

        u32 selectedSpecIndex = WFC::DrawTest( transientState->wfcSpecs, transientState->wfcGlobalState,
                                               &transientState->wfcDisplayState, pDisplay, displayDim, debugState,
                                               &transientState->wfcDisplayArena, frameMemory );

        if( selectedSpecIndex != U32MAX )
        {
            transientState->selectedSpecIndex = selectedSpecIndex;
            transientState->wfcGlobalState->cancellationRequested = true;
        }

        // Wait until cancelled or done
        if( transientState->wfcGlobalState->cancellationRequested &&
            transientState->wfcGlobalState->done )
        {
            ClearArena( &transientState->wfcArena, true );
            ClearArena( &transientState->wfcDisplayArena, true );
            transientState->wfcDisplayState = {};
            transientState->wfcDisplayState.currentSpecIndex = transientState->selectedSpecIndex;
            transientState->wfcGlobalState = nullptr;
        }
    }


    // Mesh resampling test
    // TODO Move out of the way
#if 0
    if( !editorState->testIsoSurfaceMesh || input->executableReloaded )
    {
        editorState->displayedLayer = (editorState->displayedLayer + 1) % editorState->cacheBuffers.cellsPerAxis;
        editorState->lastUpdateTimeSeconds = input->totalElapsedSeconds;
        editorState->drawingDistance = Distance( GetTranslation( editorState->testMesh.mTransform ), editorState->pCamera );
        editorState->displayedLayer = 172;

        MeshPool* meshPool = &world->meshPools[0];
        if( editorState->testIsoSurfaceMesh )
            ReleaseMesh( &editorState->testIsoSurfaceMesh );
        editorState->testIsoSurfaceMesh = ConvertToIsoSurfaceMesh( editorState->testMesh, &editorState->cacheBuffers,
                                                                  meshPool, frameMemory, renderCommands, editorState );
    }

    PushProgramChange( ShaderProgramName::FlatShading, renderCommands );
    //PushMesh( editorState->testMesh, renderCommands );
    PushMesh( *editorState->testIsoSurfaceMesh, renderCommands );
#endif

    PushProgramChange( ShaderProgramName::PlainColor, renderCommands );
    PushMaterial( nullptr, renderCommands );

#if 0
	editorState->testEditorEntity = CreateEditorEntityFor(editorState->testIsoSurfaceMesh, editorState->cacheBuffers.cellsPerAxis);
	DrawEditorEntity( editorState->testEditorEntity, editorState, renderCommands );
#endif

	DrawFloorGrid(CLUSTER_HALF_SIZE_METERS * 2, gameState->world->marchingCubeSize, renderCommands);
    DrawAxisGizmos( renderCommands );

    u16 width = renderCommands->width;
    u16 height = renderCommands->height;
    r32 elapsedSeconds = input->totalElapsedSeconds;
    DrawEditorStats( width, height, statsText, (i32)elapsedSeconds % 2 == 0 );
}
#endif
