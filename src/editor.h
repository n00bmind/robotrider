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
    v3 pCamera;
    r32 camPitch;
    r32 camYaw;
};

#endif /* __EDITOR_H__ */
