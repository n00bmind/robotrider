#ifndef __OPENGL_RENDERER_H__
#define __OPENGL_RENDERER_H__ 


// Pointers to extension functions setup natively by the platform
// TODO Is this really cross-platform?
PFNGLGETSTRINGIPROC glGetStringi;


struct OpenGLInfo
{
    b32 modernContext;

    const char *vendor;
    const char *renderer;
    const char *version;
    const char *SLversion;
    char *extensions[512];
};


#endif /* __OPENGL_RENDERER_H__ */
