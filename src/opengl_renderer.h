#ifndef __OPENGL_RENDERER_H__
#define __OPENGL_RENDERER_H__ 

#define ASSERT_GL_STATE ASSERT( glGetError() == GL_NO_ERROR);

// Pointers to extension functions setup natively by the platform
// TODO Is this really cross-platform?
PFNGLGETSTRINGIPROC	                    glGetStringi;
PFNGLGENBUFFERSPROC	                    glGenBuffers;
PFNGLBINDBUFFERPROC	                    glBindBuffer;
PFNGLBUFFERDATAPROC	                    glBufferData;
PFNGLCREATESHADERPROC	                glCreateShader;
PFNGLSHADERSOURCEPROC	                glShaderSource;
PFNGLCOMPILESHADERPROC	                glCompileShader;
PFNGLGETSHADERIVPROC	                glGetShaderiv;
PFNGLGETSHADERINFOLOGPROC	            glGetShaderInfoLog;
PFNGLCREATEPROGRAMPROC	                glCreateProgram;
PFNGLATTACHSHADERPROC	                glAttachShader;
PFNGLLINKPROGRAMPROC	                glLinkProgram;
PFNGLGETPROGRAMIVPROC	                glGetProgramiv;
PFNGLGETPROGRAMINFOLOGPROC	            glGetProgramInfoLog;
PFNGLDELETESHADERPROC	                glDeleteShader;
PFNGLGENVERTEXARRAYSPROC	            glGenVertexArrays;
PFNGLBINDVERTEXARRAYPROC	            glBindVertexArray;
PFNGLVERTEXATTRIBPOINTERPROC	        glVertexAttribPointer;
PFNGLENABLEVERTEXATTRIBARRAYPROC	    glEnableVertexAttribArray;
PFNGLUSEPROGRAMPROC	                    glUseProgram;
PFNGLGETUNIFORMLOCATIONPROC	            glGetUniformLocation;
PFNGLUNIFORMMATRIX4FVPROC	            glUniformMatrix4fv;
PFNGLDEBUGMESSAGECALLBACKARBPROC	    glDebugMessageCallbackARB;


struct OpenGLInfo
{
    b32 modernContext;

    const char *vendor;
    const char *renderer;
    const char *version;
    const char *SLversion;
    char *extensions[512];
};

struct FlyingDude
{
    v3 vertices[3];
    GLuint vao;
};

struct CubeThing
{
    v3 vertices[4];
    GLuint indices[6];
    GLuint vao;
};

#endif /* __OPENGL_RENDERER_H__ */
