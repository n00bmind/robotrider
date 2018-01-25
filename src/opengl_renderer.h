#ifndef __OPENGL_RENDERER_H__
#define __OPENGL_RENDERER_H__ 

#define ASSERT_GL_STATE ASSERT( glGetError() == GL_NO_ERROR);

struct OpenGLInfo
{
    bool modernContext;

    const char *vendor;
    const char *renderer;
    const char *version;
    const char *SLversion;
    char *extensions[512];
};

struct OpenGLImGuiState
{
    GLuint program;
    GLuint vertShader;
    GLuint fragShader;

    GLint texUniformId;
    GLint projUniformId;
    GLint pAttribId;
    GLint uvAttribId;
    GLint cAttribId;

    GLuint fontTexture;
};

struct OpenGLShaderAttribute
{
    const char *name;
};

struct OpenGLShaderUniform
{
    const char *name;
    GLint locationId;
};

struct OpenGLShaderProgram
{
    const char *vsFilename;
    const char *fsFilename;
    // Position in the array determines what location index the attribute will be bound to
    OpenGLShaderAttribute attribs[16];
    OpenGLShaderUniform uniforms[16];

    const char *vsSource;
    const char *fsSource;
    GLuint programId;
    GLuint vsId;
    GLuint fsId;
};

#define SHADERS_RELATIVE_PATH "..\\src\\shaders\\"

OpenGLShaderProgram globalShaderPrograms[] =
{
    {
        "default.vs.glsl",
        "flat.fs.glsl",
        { "pIn", "uvIn", "cIn" },
        { "mTransform" },
    },
};

struct OpenGLState
{
    GLuint vertexBuffer;
    GLuint indexBuffer;

    OpenGLImGuiState imGui;
};

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
PFNGLGETATTRIBLOCATIONPROC	            glGetAttribLocation;
PFNGLDISABLEVERTEXATTRIBARRAYPROC       glDisableVertexAttribArray;
PFNGLACTIVETEXTUREPROC	                glActiveTexture;
PFNGLBLENDEQUATIONPROC	                glBlendEquation;
PFNGLUNIFORM1IPROC	                    glUniform1i;
PFNGLBINDATTRIBLOCATIONPROC             glBindAttribLocation;
PFNGLVERTEXATTRIBIPOINTERPROC           glVertexAttribIPointer;


#endif /* __OPENGL_RENDERER_H__ */
