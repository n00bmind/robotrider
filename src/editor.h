#ifndef __EDITOR_H__
#define __EDITOR_H__ 

struct EditorEntity
{
    Mesh* mesh;
    u32 color;

    u32 cellsPerSide;
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
    r32 camZoomDelta;
    bool camOrbit;
};

struct EditorState
{
    Camera camera;
    v3 cameraWorldP;
};

#endif /* __EDITOR_H__ */
