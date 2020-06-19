#ifndef __EDITOR_H__
#define __EDITOR_H__ 

#if NON_UNITY_BUILD
#include "renderer.h"
#endif


struct EditorEntity
{
    Mesh* mesh;
    u32 color;

    i32 cellsPerSide;
};

struct EditorInput
{
    bool camLeft;
    bool camRight;
    bool camForward;
    bool camBackwards;
    bool camUp;
    bool camDown;
    r32 camPitchDelta;
    r32 camYawDelta;
    r32 camZDelta;
    bool camLookAt;
    bool camOrbit;
};

struct EditorTestsState
{
    struct ContouringSettings
    {
        // Dual Contour
        DCSettings dc;
        // Marching Cubes
        IsoSurfaceSamplingCache mcSamplingCache;
        bool mcInterpolate;

        r64 contourTimeMillis;
        r64 simplifyTimeMillis;
        r32 nextRebuildTimeSeconds;
        v3 surfaceRotDegrees;
        int currentSurfaceIndex;
        int currentTechniqueIndex;

        bool initialized;
    } contouring;

    struct
    {
        Array<WFC::Spec> specs;
        MemoryArena arena;
        MemoryArena displayArena;
        WFC::GlobalState* globalState;
        WFC::DisplayState displayState;
        i32 selectedSpecIndex;

        bool initialized;
    } wfc;

    struct
    {
        IsoSurfaceSamplingCache samplingCache;
        Mesh sampledMesh;
        Mesh* testIsoSurfaceMesh;
        EditorEntity testEditorEntity;
        r32 drawingDistance;
        i32 displayedLayer;

        bool initialized;
    } resampling;

    Mesh testMesh;
};

#define VALUES(x) \
    x(None) \
    x(Contouring) \
    x(WaveFunctionCollapse) \
    x(MeshResampling) \

STRUCT_ENUM(EditorTest, VALUES)
#undef VALUES


struct EditorState
{
    Camera camera;
    v3 cachedCameraWorldP;
    // TODO Do this smoothly according to distance to focused object
    i32 translationSpeedStep;
    bool wasOrbiting;

    EditorTestsState tests;
    EditorTest selectedTest;

    bool showImGuiDemo;
};

#endif /* __EDITOR_H__ */
