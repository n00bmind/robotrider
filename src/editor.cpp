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
CreateEditorEntityFor( Mesh* mesh, u32 cellsPerSide )
{
    EditorEntity result = { mesh, Pack01ToRGBA( 1, 0, 0, 1 ) };
    result.cellsPerSide = cellsPerSide;
    return result;
}

internal void
RenderEditorEntity( const EditorEntity& editorEntity, u32 displayedLayer, RenderCommands* renderCommands )
{
    if( editorEntity.mesh )
    {
        RenderBounds( editorEntity.mesh->bounds, editorEntity.color, renderCommands );

        aabb const& box = editorEntity.mesh->bounds;
        r32 resolutionStep = (box.max.x - box.min.x) / editorEntity.cellsPerSide;
        //RenderCubicGrid( box, resolutionStep, Pack01ToRGBA( 1, 0, 0, 0.05f ), false, renderCommands ); 
        r32 step = resolutionStep;
        u32 color = Pack01ToRGBA( 0, 0, 0, 0.3f );
        r32 xMin = box.min.x;
        r32 xMax = box.max.x;
        r32 yMin = box.min.y;
        r32 yMax = box.max.y;
        //for( u32 layer = displayedLayer; layer <= displayedLayer + 1; ++layer )
        u32 layer = displayedLayer + 1;
        {
            r32 z = box.min.z + layer * step;

            for( r32 y = yMin; y <= yMax; y += step )
                RenderLine( { xMin, y, z }, { xMax, y, z }, color, renderCommands );

            for( r32 x = xMin; x <= xMax; x += step )
            {
                RenderLine( { x, yMin, z }, { x, yMax, z }, color, renderCommands );
            }
        }
    }
}

internal void
InitMeshSamplerTest( TransientState* transientState, MemoryArena* editorArena, MemoryArena* transientArena )
{
    transientState->samplingCache = InitSurfaceSamplingCache( editorArena, V2u( 50 ) );

    TemporaryMemory tmpMemory = BeginTemporaryMemory( transientArena );
    transientState->sampledMesh = LoadOBJ( "bunny.obj", editorArena, tmpMemory, 
                                           M4Scale( V3( 10.f, 10.f, 10.f ) ) * M4Translation( V3( 0, 0, 1.f ) ) );
    EndTemporaryMemory( tmpMemory );
}

internal void
TickMeshSamplerTest( const EditorState& editorState, TransientState* transientState, MeshPool* meshPoolArray, const TemporaryMemory& frameMemory,
                     r32 elapsedT, RenderCommands* renderCommands )
{
    if( !transientState->testIsoSurfaceMesh ) //|| input.gameCodeReloaded )
    {
        transientState->displayedLayer = (transientState->displayedLayer + 1) % transientState->samplingCache.cellsPerAxis.x;
        transientState->drawingDistance = Distance( GetTranslation( transientState->sampledMesh.mTransform ), editorState.cachedCameraWorldP );
        transientState->displayedLayer = 172;

        if( transientState->testIsoSurfaceMesh )
            ReleaseMesh( &transientState->testIsoSurfaceMesh );
        transientState->testIsoSurfaceMesh = ConvertToIsoSurfaceMesh( transientState->sampledMesh, transientState->drawingDistance,
                                                                      transientState->displayedLayer, &transientState->samplingCache, meshPoolArray,
                                                                      frameMemory, renderCommands );
    }

    RenderSetShader( ShaderProgramName::FlatShading, renderCommands );
    //PushMesh( transientState->sampledMesh, renderCommands );
    RenderMesh( *transientState->testIsoSurfaceMesh, renderCommands );

    RenderSetShader( ShaderProgramName::PlainColor, renderCommands );
    RenderSetMaterial( nullptr, renderCommands );

	transientState->testEditorEntity = CreateEditorEntityFor(transientState->testIsoSurfaceMesh, transientState->samplingCache.cellsPerAxis.x);
	RenderEditorEntity( transientState->testEditorEntity, transientState->displayedLayer, renderCommands );
}

internal void
InitWFCTest( TransientState* transientState, MemoryArena* editorArena, MemoryArena* transientArena )
{
    if( !IsInitialized( transientState->wfcArena ) )
        transientState->wfcArena = MakeSubArena( transientArena, MEGABYTES(512) );
    if( !IsInitialized( transientState->wfcDisplayArena ) )
        transientState->wfcDisplayArena = MakeSubArena( transientArena, MEGABYTES(16) );

    TemporaryMemory tmpMemory = BeginTemporaryMemory( transientArena );

    transientState->wfcSpecs = LoadWFCVars( "wfc.vars", editorArena, tmpMemory );
    ASSERT( transientState->wfcSpecs.count );

    EndTemporaryMemory( tmpMemory );
}

internal void
TickWFCTest( TransientState* transientState, DebugState* debugState, const TemporaryMemory& frameMemory, RenderCommands* renderCommands )
{
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
        v2 displayP = (renderDim - displayDim) / 2.f;

        u32 selectedSpecIndex = WFC::DrawTest( transientState->wfcSpecs, transientState->wfcGlobalState,
                                               &transientState->wfcDisplayState, displayP, displayDim, debugState,
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
                for( u32 i = 0; i < genVertices.count; ++i )
                    genVertices[i].p = testVertices[i].p;
                genTriangles.count = testIndices.count / 3;
                for( u32 i = 0; i < genTriangles.count; ++i )
                {
                    genTriangles[i].v[0] = testIndices[i*3];
                    genTriangles[i].v[1] = testIndices[i*3 + 1];
                    genTriangles[i].v[2] = testIndices[i*3 + 2];
                }
                gen.vertices = genVertices;
                gen.triangles = genTriangles;
            }
            FastQuadricSimplify( &gen, (u32)(gen.triangles.count * 0.75f) );

            testVertices.count = gen.vertices.count;
            for( u32 i = 0; i < testVertices.count; ++i )
                testVertices[i].p = gen.vertices[i].p;
            testIndices.count = gen.triangles.count * 3;
            for( u32 i = 0; i < gen.triangles.count; ++i )
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
InitSurfaceContouringTest( MemoryArena* editorArena, TransientState* transientState )
{
    v2u maxCellsPerAxis = V2u( VoxelsPerClusterAxis );

    if( transientState->testMesh == nullptr )
    {
        transientState->samplingCache = InitSurfaceSamplingCache( editorArena, maxCellsPerAxis );
        InitMeshPool( &transientState->meshPool, editorArena, MEGABYTES( 256 ) );

        transientState->settings[ContouringTechnique::MarchingCubes().index].interpolate = true;
    }
}

internal void
TickSurfaceContouringTest( const GameInput& input, TransientState* transientState, RenderCommands* renderCommands, MemoryArena* tempArena )
{
    v2 renderDim = V2( renderCommands->width, renderCommands->height );
    v2 displayDim = { renderDim.x * 0.2f, renderDim.y * 0.5f };
    v2 displayP = { 100, 100 };

    ImGui::SetNextWindowPos( displayP, ImGuiCond_Always );
    ImGui::SetNextWindowSize( displayDim, ImGuiCond_Always );
    ImGui::SetNextWindowSizeConstraints( ImVec2( -1, -1 ), ImVec2( -1, -1 ) );

    ImGui::Begin( "window_contour", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove );

    bool techniqueChanged = ImGui::Combo( "Algorithm", &transientState->currentTechniqueIndex, ContouringTechnique::Values::names,
                                          ContouringTechnique::Values::count );
    
    ContouringSettings& currentSettings = transientState->settings[transientState->currentTechniqueIndex];
    ContouringSettings settings = currentSettings;

    switch( transientState->currentTechniqueIndex )
    {
        case ContouringTechnique::MarchingCubes().index:
        {
            ImGui::Checkbox( "Interpolate", &settings.interpolate );
        } break;
    }

    if( transientState->testMesh == nullptr || input.gameCodeReloaded || techniqueChanged || !EQUAL( settings, currentSettings ) )
    {
        r64 start = globalPlatform.DEBUGCurrentTimeMillis();
        switch( transientState->currentTechniqueIndex )
        {
            case ContouringTechnique::MarchingCubes().index:
            {
                transientState->testMesh = MarchAreaFast( { V3Zero, V3iZero }, V3( ClusterSizeMeters ), VoxelSizeMeters, TorusSurfaceFunc, nullptr,
                                                          &transientState->samplingCache, &transientState->meshPool, settings.interpolate );
               
            } break;
            case ContouringTechnique::DualContouring().index:
            {
                transientState->testMesh = DCArea( { V3Zero, V3iZero }, V3( ClusterSizeMeters ), VoxelSizeMeters, TorusSurfaceFunc, nullptr,
                                                   &transientState->meshPool, tempArena, DCSettings() );

            } break;
        }
        transientState->elapsedMillis = globalPlatform.DEBUGCurrentTimeMillis() - start;
    }

    COPY( settings, currentSettings );

    ImGui::Dummy( { 0, 20 } );
    ImGui::Separator();
    u32 c = (u32)ClusterSizeMeters;
    ImGui::Text( "%u x %u x %u cells", c, c, c );
    ImGui::Dummy( { 0, 20 } );
    ImGui::Text( "Tris: %u", transientState->testMesh->indexCount / 3 );
    ImGui::Text( "Verts: %u", transientState->testMesh->vertexCount );
    ImGui::Dummy( { 0, 20 } );
    ImGui::Text( "Extracted in: %g millis.", transientState->elapsedMillis );
    ImGui::End();

    RenderSetShader( ShaderProgramName::FlatShading, renderCommands );
    RenderMesh( *transientState->testMesh, renderCommands );
}

void
InitEditor( const v2i screenDim, GameState* gameState, EditorState* editorState, TransientState* transientState,
            MemoryArena* editorArena, MemoryArena* transientArena )
{
#if 0
    InitMeshSamplerTest( transientState, editorArena, transientArena );

    InitWFCTest( transientState, editorArena, transientArena );
#endif

    InitSurfaceContouringTest( editorArena, transientState );
}

void
UpdateAndRenderEditor( const GameInput& input, GameState* gameState, TransientState* transientState, 
                       DebugState* debugState, RenderCommands *renderCommands, const char* statsText, 
                       const TemporaryMemory& frameMemory )
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
        editorState->camera.worldToCamera = mLookAt;
        editorState->translationSpeedStep = 2;
    }

    // Update camera based on input
    {
        editorState->translationSpeedStep += (int)editorInput.camZDelta;
        Clamp( &editorState->translationSpeedStep, 0, 2 );

        r32 camMovementSpeed = Pow( 10, (r32)editorState->translationSpeedStep );
        r32 camRotationSpeed = 1.f;

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

        r32 camPitchDelta = 0, camYawDelta = 0;
        if( editorInput.camPitchDelta )
            camPitchDelta += editorInput.camPitchDelta * camRotationSpeed * dT;
        if( editorInput.camYawDelta )
            camYawDelta += editorInput.camYawDelta * camRotationSpeed * dT; 


        m4& worldToCamera = editorState->camera.worldToCamera;
        m4 cameraToWorld = Transposed( worldToCamera );

        // Find camera position in the world
        v3 cameraWorldP = -(cameraToWorld * GetTranslation( worldToCamera ));

        if( editorInput.camLookAt )
        {
            // Rotate around camera X axis
            m4 localXRotation = cameraToWorld * M4XRotation( camPitchDelta );
            worldToCamera = Transposed( localXRotation );

            // Rotate around world Z axis
            m4 worldZRotation = M4Translation( cameraWorldP ) * M4ZRotation( camYawDelta ) * M4Translation( -cameraWorldP );
            worldToCamera *= worldZRotation;
        }
        else if( editorInput.camOrbit )
        {
            if( !editorState->wasOrbiting )
            {
                v3 cameraTargetP = V3Zero; // For now
                worldToCamera = M4CameraLookAt( cameraWorldP, cameraTargetP, V3Up );
                cameraToWorld = Transposed( worldToCamera );
            }

            // Orbit around world axes
            worldToCamera *= M4AxisAngle( GetCameraBasisX( worldToCamera ), -camPitchDelta );
            worldToCamera *= M4ZRotation( camYawDelta );

            camTranslationDelta = Hadamard( camTranslationDelta, { 0, 0, 1 } );
        }

        // Apply translation (already in camera space)
        Translate( worldToCamera, -camTranslationDelta );

        renderCommands->camera = editorState->camera;

        editorState->cachedCameraWorldP = cameraWorldP;
        editorState->wasOrbiting = editorInput.camOrbit;
    }

#if 0
    // Resampling meshes using marching cubes
    TickMeshSamplerTest( *editorState, transientState, &world->meshPools[0], frameMemory, elapsedT, renderCommands );
    // Mesh simplification
    TickMeshSimplifierTest();

    // Wave function collapse algorithm
    TickWFCTest( transientState, debugState, frameMemory, renderCommands );
#endif

    TickSurfaceContouringTest( input, transientState, renderCommands, frameMemory.arena );

    RenderSetShader( ShaderProgramName::PlainColor, renderCommands );
    RenderSetMaterial( nullptr, renderCommands );

	//RenderFloorGrid( ClusterSizeMeters, 10.f, renderCommands );
    DrawAxisGizmos( renderCommands );

    u16 width = renderCommands->width;
    u16 height = renderCommands->height;
    DrawEditorStateWindow( V2u( width - 280, 50 ), V2u( 250, height - 900 ), *editorState );

    DrawEditorStats( width, height, statsText, (i32)elapsedT % 2 == 0 );
}
#endif
