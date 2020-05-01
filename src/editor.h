#ifndef __EDITOR_H__
#define __EDITOR_H__ 

#if NON_UNITY_BUILD
#include "renderer.h"
//#include "robotrider.h"
//#include "ui.h"
#endif


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
    r32 camZDelta;
    bool camOrbit;
};

struct EditorState
{
    Camera camera;
    v3 cachedCameraWorldP;
    i32 translationSpeedStep;
    bool wasOrbiting;
};

#endif /* __EDITOR_H__ */
