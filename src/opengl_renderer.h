/*
The MIT License

Copyright (c) 2017 Oscar Peñas Pariente <oscarpp80@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#ifndef __OPENGL_RENDERER_H__
#define __OPENGL_RENDERER_H__ 

#define ASSERT_GL_STATE ASSERT( glGetError() == GL_NO_ERROR );

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

#define MAX_SHADER_ATTRIBS 16
#define MAX_SHADER_UNIFORMS 16

// FIXME Don't have this in a global define!
#define SHADERS_RELATIVE_PATH "..\\src\\shaders\\"

struct OpenGLShaderProgram
{
    ShaderProgramName name;
    const char* vsFilename;
    const char* gsFilename;
    const char* fsFilename;
    // Position in the array determines what location index the attribute will be bound to
    OpenGLShaderAttribute attribs[MAX_SHADER_ATTRIBS];
    OpenGLShaderUniform uniforms[MAX_SHADER_UNIFORMS];

    const char* vsSource;
    const char* gsSource;
    const char* fsSource;
    GLuint programId;
    GLuint vsId;
    GLuint gsId;
    GLuint fsId;
};

struct OpenGLState
{
    GLuint vertexBuffer;
    GLuint indexBuffer;
    m4 mCurrentProjView;

    OpenGLImGuiState imGui;

    OpenGLShaderProgram *activeProgram;
    u32 white;
    void* whiteTexture;

    GLuint queryObjectId;
};


// Pointers to extension functions setup natively by the platform
// TODO Is this really cross-platform?
PFNGLGETSTRINGIPROC	                    glGetStringi;
PFNGLGENBUFFERSPROC	                    glGenBuffers;
PFNGLBINDBUFFERPROC	                    glBindBuffer;
PFNGLBUFFERDATAPROC	                    glBufferData;
PFNGLBUFFERSUBDATAPROC                  glBufferSubData;
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
PFNGLGENERATEMIPMAPPROC                 glGenerateMipmap;
PFNGLGENQUERIESPROC                     glGenQueries;
PFNGLBEGINQUERYPROC                     glBeginQuery;
PFNGLENDQUERYPROC                       glEndQuery;
PFNGLGETQUERYOBJECTUIVPROC              glGetQueryObjectuiv;


#endif /* __OPENGL_RENDERER_H__ */
