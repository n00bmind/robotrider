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
    v3 pCamera;
    r32 camPitch;
    r32 camYaw;
};

#endif /* __EDITOR_H__ */
