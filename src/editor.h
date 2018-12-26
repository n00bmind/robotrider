#ifndef __EDITOR_H__
#define __EDITOR_H__ 

struct EditorEntity
{
    Mesh* mesh;
    u32 color;

    u32 cellsPerSide;
};

struct EditorState
{
    r32 lastUpdateTimeSeconds;

    v3 pCamera;
    r32 camPitch;
    r32 camYaw;

    MarchingCacheBuffers cacheBuffers;

    // Mesh resampling test
#if 0
    Mesh testMesh;
    Mesh* testIsoSurfaceMesh;
    EditorEntity testEditorEntity;
#endif

    Array<WFC::Spec> wfcSpecs;
    u32 selectedSpecIndex;
    MemoryArena wfcArena;
    MemoryArena wfcDisplayArena;
    WFC::JobsInfo* wfcJobsInfo;
    WFC::DisplayState wfcDisplayState;

    r32 drawingDistance;
    u32 displayedLayer;
};

#endif /* __EDITOR_H__ */
