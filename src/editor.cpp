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
#if NON_UNITY_BUILD
#include "editor.h"
#include "renderer.h"
#include "robotrider.h" 
#include "meshgen.h"
#include "asset_loaders.h"
#include "wfc.h"
#include "game.h"
#include "ui.h"
#endif

#if !RELEASE

internal EditorEntity
CreateEditorEntityFor( Mesh* mesh, int cellsPerSide )
{
    EditorEntity result = { mesh, Pack01ToRGBA( 1, 0, 0, 1 ) };
    result.cellsPerSide = cellsPerSide;
    return result;
}

internal void
RenderEditorEntity( const EditorEntity& editorEntity, int displayedLayer, RenderCommands* renderCommands )
{
    if( editorEntity.mesh )
    {
        RenderBounds( editorEntity.mesh->bounds, editorEntity.color, renderCommands );

        v3 min, max;
        MinMax( editorEntity.mesh->bounds, &min, &max );

        f32 resolutionStep = (max.x - min.x) / editorEntity.cellsPerSide;
        //RenderCubicGrid( box, resolutionStep, Pack01ToRGBA( 1, 0, 0, 0.05f ), false, renderCommands ); 
        f32 step = resolutionStep;
        u32 color = Pack01ToRGBA( 0, 0, 0, 0.3f );
        f32 xMin = min.x;
        f32 xMax = max.x;
        f32 yMin = min.y;
        f32 yMax = max.y;
        //for( int layer = displayedLayer; layer <= displayedLayer + 1; ++layer )
        int layer = displayedLayer + 1;
        {
            f32 z = min.z + layer * step;

            for( f32 y = yMin; y <= yMax; y += step )
                RenderLine( { xMin, y, z }, { xMax, y, z }, color, renderCommands );

            for( f32 x = xMin; x <= xMax; x += step )
            {
                RenderLine( { x, yMin, z }, { x, yMax, z }, color, renderCommands );
            }
        }
    }
}

internal void
InitMeshSamplerTest( EditorState* state, MemoryArena* editorArena, MemoryArena* transientArena )
{
    state->tests.resampling.samplingCache = InitSurfaceSamplingCache( editorArena, V2i( 50 ) );

    TemporaryMemory tmpMemory = BeginTemporaryMemory( transientArena );
    state->tests.resampling.sampledMesh = LoadOBJ( "bunny.obj", editorArena, tmpMemory, 
                                           M4Scale( V3( 10.f, 10.f, 10.f ) ) * M4Translation( V3( 0.f, 0.f, 1.f ) ) );
    EndTemporaryMemory( tmpMemory );
}

internal void
TickMeshSamplerTest( EditorState* state, MeshPool* meshPoolArray, MemoryArena* editorArena, MemoryArena* transientArena,
                     f32 elapsedT, RenderCommands* renderCommands )
{
    if( !state->tests.resampling.initialized )
    {
        InitMeshSamplerTest( state, editorArena, transientArena );
        state->tests.resampling.initialized = true;
    }

    if( !state->tests.resampling.testIsoSurfaceMesh ) //|| input.gameCodeReloaded )
    {
        state->tests.resampling.displayedLayer = (state->tests.resampling.displayedLayer + 1) % state->tests.resampling.samplingCache.cellsPerAxis.x;
        state->tests.resampling.drawingDistance = DistanceFast( GetTranslation( state->tests.resampling.sampledMesh.mTransform ), state->cachedCameraWorldP );
        state->tests.resampling.displayedLayer = 172;

        if( state->tests.resampling.testIsoSurfaceMesh )
            ReleaseMesh( &state->tests.resampling.testIsoSurfaceMesh );
        state->tests.resampling.testIsoSurfaceMesh =
            ConvertToIsoSurfaceMesh( state->tests.resampling.sampledMesh, state->tests.resampling.drawingDistance, state->tests.resampling.displayedLayer,
                                     &state->tests.resampling.samplingCache, meshPoolArray, transientArena, renderCommands );
    }

    RenderSetShader( ShaderProgramName::FlatShading, renderCommands );
    //PushMesh( state->tests.resampling.sampledMesh, renderCommands );
    RenderMesh( *state->tests.resampling.testIsoSurfaceMesh, renderCommands );

    RenderSetShader( ShaderProgramName::PlainColor, renderCommands );
    RenderSetMaterial( nullptr, renderCommands );

	state->tests.resampling.testEditorEntity = CreateEditorEntityFor(state->tests.resampling.testIsoSurfaceMesh, state->tests.resampling.samplingCache.cellsPerAxis.x);
	RenderEditorEntity( state->tests.resampling.testEditorEntity, state->tests.resampling.displayedLayer, renderCommands );
}

internal void
InitWFCTest( EditorState* state, MemoryArena* editorArena, MemoryArena* transientArena )
{
    if( !IsInitialized( state->tests.wfc.arena ) )
        state->tests.wfc.arena = MakeSubArena( editorArena, MEGABYTES(512) );
    if( !IsInitialized( state->tests.wfc.displayArena ) )
        state->tests.wfc.displayArena = MakeSubArena( editorArena, MEGABYTES(16) );

    TemporaryMemory tmpMemory = BeginTemporaryMemory( transientArena );

    state->tests.wfc.specs = LoadWFCVars( "wfc.vars", editorArena, tmpMemory );
    ASSERT( state->tests.wfc.specs.count );

    EndTemporaryMemory( tmpMemory );
}

internal void
TickWFCTest( EditorState* state, DebugState* debugState, MemoryArena* editorArena, MemoryArena* transientArena,
             RenderCommands* renderCommands )
{
    if( !state->tests.wfc.initialized )
    {
        InitWFCTest( state, editorArena, transientArena );
        state->tests.wfc.initialized = true;
    }

    if( !state->tests.wfc.globalState )
    {
        WFC::Spec& spec = state->tests.wfc.specs[state->tests.wfc.displayState.currentSpecIndex];
        WFC::Spec defaultSpec = WFC::DefaultSpec();
        // Ignore certain attributes
        spec.outputChunkDim = defaultSpec.outputChunkDim;
        spec.periodic = defaultSpec.periodic;

        state->tests.wfc.globalState = WFC::StartWFCAsync( spec, { 0, 0 }, &state->tests.wfc.arena );
    }

    WFC::UpdateWFCAsync( state->tests.wfc.globalState );
    {
        v2 renderDim = V2( renderCommands->width, renderCommands->height );
        v2 displayDim = renderDim * 0.9f;
        v2 displayP = (renderDim - displayDim) / 2.f;

        int selectedSpecIndex = WFC::DrawTest( state->tests.wfc.specs, state->tests.wfc.globalState,
                                               &state->tests.wfc.displayState, displayP, displayDim, debugState,
                                               &state->tests.wfc.displayArena, transientArena );

        if( selectedSpecIndex != -1 )
        {
            state->tests.wfc.selectedSpecIndex = selectedSpecIndex;
            state->tests.wfc.globalState->cancellationRequested = true;
        }

        // Wait until cancelled or done
        if( state->tests.wfc.globalState->cancellationRequested &&
            state->tests.wfc.globalState->done )
        {
            ClearArena( &state->tests.wfc.arena, true );
            ClearArena( &state->tests.wfc.displayArena, true );
            state->tests.wfc.displayState = {};
            state->tests.wfc.displayState.currentSpecIndex = state->tests.wfc.selectedSpecIndex;
            state->tests.wfc.globalState = nullptr;
        }
    }
}

#if 0
internal void
TickMeshSimplifierTest()
{
    // Mesh simplification test
    // TODO Update
    Mesh testMesh;
    {
        if( ((i32)input.frameCounter - 180) % 300 == 0 )
        {
            FQSMesh gen;
            {
                genVertices.count = testVertices.count;
                for( int i = 0; i < genVertices.count; ++i )
                    genVertices[i].p = testVertices[i].p;
                genTriangles.count = testIndices.count / 3;
                for( int i = 0; i < genTriangles.count; ++i )
                {
                    genTriangles[i].v[0] = testIndices[i*3];
                    genTriangles[i].v[1] = testIndices[i*3 + 1];
                    genTriangles[i].v[2] = testIndices[i*3 + 2];
                }
                gen.vertices = genVertices;
                gen.triangles = genTriangles;
            }
            FastQuadricSimplify( &gen, (int)(gen.triangles.count * 0.75f) );

            testVertices.count = gen.vertices.count;
            for( int i = 0; i < testVertices.count; ++i )
                testVertices[i].p = gen.vertices[i].p;
            testIndices.count = gen.triangles.count * 3;
            for( int i = 0; i < gen.triangles.count; ++i )
            {
                testIndices[i*3 + 0] = gen.triangles[i].v[0];
                testIndices[i*3 + 1] = gen.triangles[i].v[1];
                testIndices[i*3 + 2] = gen.triangles[i].v[2];
            }
        }

        testMesh.vertices = testVertices.data;
        testMesh.indices = testIndices.data;
        testMesh.vertexCount = testVertices.count;
        testMesh.indexCount = testIndices.count;
        testMesh.mTransform = Scale({ 10, 10, 10 });
    }
    PushMesh( testMesh, renderCommands );
}
#endif

internal void
InitSurfaceContouringTest( EditorState* state, MemoryArena* editorArena )
{
    v2i maxCellsPerAxis = V2i( VoxelsPerClusterAxis );
    state->tests.contouring.mcSamplingCache = InitSurfaceSamplingCache( editorArena, maxCellsPerAxis );

    state->tests.contouring.mcInterpolate = true;
    state->tests.contouring.dc.cellPointsComputationMethod = DCComputeMethod::QEFProbabilistic;
    state->tests.contouring.dc.sigmaN = 0.1f;
    state->tests.contouring.dc.sigmaNDouble = 0.01f;
}

internal void
TickSurfaceContouringTest( const GameInput& input, EditorState* state, RenderCommands* renderCommands,
                           MemoryArena* editorArena, MemoryArena* tempArena )
{
    v2 renderDim = V2( renderCommands->width, renderCommands->height );
    v2 displayDim = { renderDim.x * 0.2f, renderDim.y * 0.5f };
    v2 displayP = { 100, 100 };

    SamplingData samplingData = {};

    EditorTestsState::ContouringSettings& currentSettings = state->tests.contouring;
    if( !currentSettings.initialized )
    {
        InitSurfaceContouringTest( state, editorArena );
        currentSettings.initialized = true;
    }

    EditorTestsState::ContouringSettings settings = currentSettings;

    ImGui::SetNextWindowPos( displayP, ImGuiCond_Always );
    ImGui::SetNextWindowSize( displayDim, ImGuiCond_Always );
    ImGui::SetNextWindowSizeConstraints( ImVec2( -1, -1 ), ImVec2( -1, -1 ) );

    ImGui::Begin( "window_contour", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove );

    ImGui::Combo( "Surface", &settings.currentSurfaceIndex, SimpleSurface::Values::names, SimpleSurface::Values::count );
    samplingData.surfaceType = settings.currentSurfaceIndex;

    static const f32 minus180 = -180.f;
    static const f32 plus180 = 180.f;
    ImGui::DragFloat( "X Rotation", &settings.surfaceRotDegrees.x, 0.1f, minus180, plus180, "%.1f deg." );
    ImGui::DragFloat( "Y Rotation", &settings.surfaceRotDegrees.y, 0.1f, minus180, plus180, "%.1f deg." );
    ImGui::DragFloat( "Z Rotation", &settings.surfaceRotDegrees.z, 0.1f, minus180, plus180, "%.1f deg." );
    m4 invRotation = M4ZRotation( -Radians( settings.surfaceRotDegrees.z ) )
        * M4YRotation( -Radians( settings.surfaceRotDegrees.y ) )
        * M4XRotation( -Radians( settings.surfaceRotDegrees.x ) );
    samplingData.invWorldTransform = invRotation;

    ImGui::Dummy( { 0, 20 } );
    ImGui::Combo( "Algorithm", &settings.currentTechniqueIndex, ContouringTechnique::Values::names,
                  ContouringTechnique::Values::count );
    
    switch( settings.currentTechniqueIndex )
    {
        case ContouringTechnique::MarchingCubes().index:
        {
            ImGui::Checkbox( "Interpolate edge points", &settings.mcInterpolate );
        } break;
        case ContouringTechnique::DualContouring().index:
        {
            static const f32 sigmaMin = 0.001f;
            static const f32 sigmaMinDouble = 0.00001f;
            static const f32 sigmaMax = 100.f;

            ImGui::RadioButton( "Average", (int*)&settings.dc.cellPointsComputationMethod, (int)DCComputeMethod::Average );
            ImGui::RadioButton( "QEFClassic", (int*)&settings.dc.cellPointsComputationMethod, (int)DCComputeMethod::QEFClassic );
            ImGui::RadioButton( "QEFProbabilistic", (int*)&settings.dc.cellPointsComputationMethod, (int)DCComputeMethod::QEFProbabilistic );
            ImGui::SameLine( 250 );
            ImGui::PushItemWidth( -100 );
            ImGui::SliderFloat( "Sigma N##sigma_n", &settings.dc.sigmaN, sigmaMin, sigmaMax, "%.3f", 10.f );
            ImGui::PopItemWidth();
            ImGui::RadioButton( "QEFProbabilisticDouble", (int*)&settings.dc.cellPointsComputationMethod, (int)DCComputeMethod::QEFProbabilisticDouble );
            ImGui::SameLine( 250 );
            ImGui::PushItemWidth( -100 );
            ImGui::SliderFloat( "Sigma N##sigma_n_double", &settings.dc.sigmaNDouble, sigmaMinDouble, sigmaMax, "%.5f", 10.f );
            ImGui::PopItemWidth();

            ImGui::Dummy( { 0, 20 } );
            ImGui::Checkbox( "Clamp cell points", &settings.dc.clampCellPoints );
            ImGui::Checkbox( "Approximate edge intersections", &settings.dc.approximateEdgeIntersection );
        } break;
    }

    // Rebuild after a delay if settings change
    if( !EQUAL( settings, currentSettings ) )
    {
        COPY( settings, currentSettings );
        currentSettings.nextRebuildTimeSeconds = input.totalElapsedSeconds + 0.5f;
    }

    if( Empty( state->tests.testMesh ) || input.gameCodeReloaded
        || (currentSettings.nextRebuildTimeSeconds && input.totalElapsedSeconds > currentSettings.nextRebuildTimeSeconds) )
    {
        BucketArray<TexturedVertex> tmpVertices( tempArena, 1024, Temporary() );
        BucketArray<i32> tmpIndices( tempArena, 1024, Temporary() );

        f64 start = globalPlatform.DEBUGCurrentTimeMillis();

        switch( settings.currentTechniqueIndex )
        {
            case ContouringTechnique::MarchingCubes().index:
            {
                MarchVolumeFast( { V3Zero, V3iZero }, V3( ClusterSizeMeters ), VoxelSizeMeters, SimpleSurfaceFunc, &samplingData,
                                 &currentSettings.mcSamplingCache, &tmpVertices, &tmpIndices, settings.mcInterpolate );
               
            } break;
            case ContouringTechnique::DualContouring().index:
            {
                DCVolume( { V3Zero, V3iZero }, V3( ClusterSizeMeters ), VoxelSizeMeters, SimpleSurfaceFunc, &samplingData,
                          &tmpVertices, &tmpIndices, editorArena, tempArena, settings.dc );

            } break;
        }
        state->tests.testMesh = CreateMeshFromBuffers( tmpVertices, tmpIndices, editorArena );
        currentSettings.contourTimeMillis = globalPlatform.DEBUGCurrentTimeMillis() - start;

        start = globalPlatform.DEBUGCurrentTimeMillis();

#if 0
        // Simplify mesh
        TemporaryMemory tempMemory = BeginTemporaryMemory( tempArena );

        FQSMesh fqsMesh = CreateFQSMesh( state->tests.testMesh.vertices, state->tests.testMesh.indices, tempMemory );
        FastQuadricSimplify( &fqsMesh, 10000, tempMemory );

        state->tests.testMesh.vertices.Resize( fqsMesh.vertices.count );
        for( int i = 0; i < fqsMesh.vertices.count; ++i )
            // FIXME Vertex attributes
            state->tests.testMesh.vertices[i].p = fqsMesh.vertices[i].p;
        state->tests.testMesh.indices.Resize( fqsMesh.triangles.count * 3 );
        for( int i = 0; i < fqsMesh.triangles.count; ++i )
        {
            state->tests.testMesh.indices[ i*3 + 0 ] = fqsMesh.triangles[i].v[0];
            state->tests.testMesh.indices[ i*3 + 1 ] = fqsMesh.triangles[i].v[1];
            state->tests.testMesh.indices[ i*3 + 2 ] = fqsMesh.triangles[i].v[2];
        }
        EndTemporaryMemory( tempMemory );

        currentSettings.simplifyTimeMillis = globalPlatform.DEBUGCurrentTimeMillis() - start;
#endif

        currentSettings.nextRebuildTimeSeconds = 0.f;
    }

    ImGui::Dummy( { 0, 20 } );
    ImGui::Separator();
    int c = (int)ClusterSizeMeters;
    ImGui::Text( "%u x %u x %u cells", c, c, c );
    ImGui::Dummy( { 0, 20 } );
    ImGui::Text( "Tris: %u", state->tests.testMesh.indices.count / 3 );
    ImGui::Text( "Verts: %u", state->tests.testMesh.vertices.count );
    ImGui::Dummy( { 0, 20 } );
    ImGui::Text( "Extracted in: %g millis.", currentSettings.contourTimeMillis );
    ImGui::Text( "Simplified in: %g millis.", currentSettings.simplifyTimeMillis );
    ImGui::End();

    RenderSwitch( RenderSwitchType::Culling, false, renderCommands );
    RenderSetShader( ShaderProgramName::FlatShading, renderCommands );
    RenderMesh( state->tests.testMesh, renderCommands );

    RenderSwitch( RenderSwitchType::Culling, true, renderCommands );
}

internal EditorInput
MapGameInputToEditorInput( const GameInput& input )
{
    EditorInput result = {};
    const GameControllerInput& input0 = GetController( input, 0 );

    result.camLeft      = input.keyMouse.keysDown[KeyA] || input0.dLeft.endedDown;
    result.camRight     = input.keyMouse.keysDown[KeyD] || input0.dRight.endedDown;
    result.camForward   = input.keyMouse.keysDown[KeyW] || input0.dUp.endedDown;
    result.camBackwards = input.keyMouse.keysDown[KeyS] || input0.dDown.endedDown;
    result.camUp        = input.keyMouse.keysDown[KeySpace] || input0.rightShoulder.endedDown;
    result.camDown      = input.keyMouse.keysDown[KeyLeftControl] || input0.leftShoulder.endedDown;

    result.camYawDelta = input.keyMouse.mouseRawXDelta
        ? input.keyMouse.mouseRawXDelta : input0.rightStick.avgX;
    result.camPitchDelta = input.keyMouse.mouseRawYDelta
        ? -input.keyMouse.mouseRawYDelta : input0.rightStick.avgY;
    result.camLookAt = input.keyMouse.mouseButtons[MouseButtonRight].endedDown || input0.rightThumb.endedDown;
    result.camOrbit = input.keyMouse.keysDown[KeyLeftShift];
    // TODO Progressive zoom
    result.camZDelta = input.keyMouse.mouseRawZDelta;

    return result;
}

void
InitEditor( const v2i screenDim, GameState* gameState, EditorState* editorState, TransientState* transientState,
            MemoryArena* editorArena, MemoryArena* transientArena )
{
#if 0

#endif
}

void
UpdateAndRenderEditor( const GameInput& input, GameState* gameState, TransientState* transientState, 
                       DebugState* debugState, RenderCommands *renderCommands, const char* statsText, 
                       MemoryArena* editorArena, MemoryArena* transientArena )
{
    float dT = input.frameElapsedSeconds;
    float elapsedT = input.totalElapsedSeconds;

    EditorState* editorState = &gameState->DEBUGeditorState;
    World* world = gameState->world;

    EditorInput editorInput = MapGameInputToEditorInput( input );

    if( editorState->cachedCameraWorldP == V3Zero )
    {
        v3 pCamera = V3( 0.f, -150.f, 220.f );
        m4 mLookAt = M4CameraLookAt( pCamera, world->pPlayer, V3Up );

        editorState->camera = DefaultCamera();
        editorState->camera.cameraFromWorld = mLookAt;
        editorState->translationSpeedStep = 2;
    }

    // Update camera based on input
    {
        editorState->translationSpeedStep += (int)editorInput.camZDelta;
        Clamp( &editorState->translationSpeedStep, 0, 2 );

        f32 camMovementSpeed = Pow( 10, (f32)editorState->translationSpeedStep );
        f32 camRotationSpeed = 0.05f;

        v3 camTranslationDelta = {};
        if( editorInput.camLeft )
            camTranslationDelta.x -= camMovementSpeed * dT;
        if( editorInput.camRight )
            camTranslationDelta.x += camMovementSpeed * dT;

        if( editorInput.camForward )
            camTranslationDelta.z -= camMovementSpeed * dT;
        if( editorInput.camBackwards )
            camTranslationDelta.z += camMovementSpeed * dT;

        if( editorInput.camDown )
            camTranslationDelta.y -= camMovementSpeed * dT;
        if( editorInput.camUp )
            camTranslationDelta.y += camMovementSpeed * dT;

        f32 camPitchDelta = 0, camYawDelta = 0;
        if( editorInput.camPitchDelta )
            camPitchDelta += editorInput.camPitchDelta * camRotationSpeed * dT;
        if( editorInput.camYawDelta )
            camYawDelta += editorInput.camYawDelta * camRotationSpeed * dT; 


        m4& cameraFromWorld = editorState->camera.cameraFromWorld;
        m4 worldFromCamera = Transposed( cameraFromWorld );

        // Find camera position in the world
        v3 cameraWorldP = -(worldFromCamera * GetTranslation( cameraFromWorld ));

        if( editorInput.camLookAt )
        {
            // Rotate around camera X axis
            m4 localXRotation = worldFromCamera * M4XRotation( camPitchDelta );
            cameraFromWorld = Transposed( localXRotation );

            // Rotate around world Z axis
            m4 worldZRotation = M4Translation( cameraWorldP ) * M4ZRotation( camYawDelta ) * M4Translation( -cameraWorldP );
            cameraFromWorld *= worldZRotation;
        }
        else if( editorInput.camOrbit )
        {
            if( !editorState->wasOrbiting )
            {
                v3 cameraTargetP = V3Zero; // For now
                cameraFromWorld = M4CameraLookAt( cameraWorldP, cameraTargetP, V3Up );
                worldFromCamera = Transposed( cameraFromWorld );
            }

            // Orbit around world axes
            cameraFromWorld *= M4AxisAngle( GetCameraBasisX( cameraFromWorld ), -camPitchDelta );
            cameraFromWorld *= M4ZRotation( camYawDelta );

            camTranslationDelta = Hadamard( camTranslationDelta, { 0, 0, 1 } );
        }

        // Apply translation (already in camera space)
        Translate( cameraFromWorld, -camTranslationDelta );

        renderCommands->camera = editorState->camera;

        editorState->cachedCameraWorldP = cameraWorldP;
        editorState->wasOrbiting = editorInput.camOrbit;

        RenderCamera( cameraFromWorld, renderCommands );
    }

    // Menu bar
    if( ImGui::BeginMainMenuBar() )
    {
        if( ImGui::BeginMenu("Tests") )
        {
            for( int i = 0; i < EditorTest::Values::count; ++i )
            {
                EditorTest const& entry = EditorTest::Values::items[i];
                if( ImGui::MenuItem( EditorTest::Values::names[i], nullptr, editorState->selectedTest == entry ) )
                    editorState->selectedTest = entry;
            }

            ImGui::Separator();
            ImGui::MenuItem( "ImGui demo", nullptr, &editorState->showImGuiDemo );

            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    switch( editorState->selectedTest.index )
    {
        case EditorTest::Contouring().index:
            TickSurfaceContouringTest( input, editorState, renderCommands, editorArena, transientArena );
            break;
        case EditorTest::WaveFunctionCollapse().index:
            TickWFCTest( editorState, debugState, editorArena, transientArena, renderCommands );
            break;
        case EditorTest::MeshResampling().index:
            // Resampling meshes using marching cubes
            TickMeshSamplerTest( editorState, &world->meshPools[0], editorArena, transientArena, elapsedT, renderCommands );
            break;
#if 0
        case EditorTest::MeshDecimation().index:
            TickMeshSimplifierTest();
            break;
#endif
    }

    if( editorState->showImGuiDemo )
        ImGui::ShowDemoWindow();


    RenderSetShader( ShaderProgramName::PlainColor, renderCommands );
    RenderSetMaterial( nullptr, renderCommands );

    // Render current cluster limits
    u32 red = Pack01ToRGBA( 1, 0, 0, 1 );
    RenderBoundsAt( V3Zero, ClusterSizeMeters, red, renderCommands );

	//RenderFloorGrid( ClusterSizeMeters, 10.f, renderCommands );
    DrawAxisGizmos( renderCommands );

    u16 width = renderCommands->width;
    u16 height = renderCommands->height;
    DrawEditorStateWindow( V2i( width - 280, 50 ), V2i( 250, height - 900 ), *editorState );

    DrawEditorStats( width, height, statsText, (i32)elapsedT % 2 == 0 );
}
#endif
