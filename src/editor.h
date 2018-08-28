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

    Mesh testMesh;
    Mesh* testIsoSurfaceMesh;
    EditorEntity testEditorEntity;

    r32 drawingDistance;
    u32 displayedLayer;
};

#endif /* __EDITOR_H__ */
