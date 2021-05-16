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

#if NON_UNITY_BUILD
#include "opengl_renderer.h"
#include "platform.h"
#include "game.h"
#include "imgui/imgui.h"
#endif


// TODO Add a list of #defines as a string built on the fly with values from the code to every program
// (this would include the vertex format(s) that the programs will use, together with uniforms and all input essentially)
// TODO Unify all stages in a single file per program, using an approach like in https://community.khronos.org/t/vertex-and-fragment-shaders-in-same-file/49949/3
internal
OpenGLShaderProgram globalShaderPrograms[] =
{
    {
        ShaderProgramName::PlainColor,
        "default.vs.glsl",
        nullptr,
        "plain_color.fs.glsl",
        { "inPosition", "inTexCoords", "inColor" },
        { { "mTransform" }, { "simClusterOffsets" }, { "simClusterIndex" }, },
    },
    {
        ShaderProgramName::PlainColorVoxel,
        "default_instanced.vs.glsl",
        nullptr,
        "plain_color.fs.glsl",
        { "inPosition", "inTexCoords", "inColor", "inInstanceOffset" },
        { { "mTransform" }, { "simClusterOffsets" }, { "simClusterIndex" }, },
    },
    {
        ShaderProgramName::FlatShading,
        "default.vs.glsl",
        "face_normal.gs.glsl",
        //nullptr,
        "flat.fs.glsl",
        { "inPosition", "inTexCoords", "inColor" },
        { { "mTransform" }, { "simClusterOffsets" }, { "simClusterIndex" }, },
    },
};


GL_DEBUG_CALLBACK(OpenGLDebugCallback)
{
    // Filter notifications by default because they flood the logs with some cards..
    if( severity == GL_DEBUG_SEVERITY_NOTIFICATION )
        return;

    const char *sourceStr;
    switch( source )
    {
        case GL_DEBUG_SOURCE_API:	            sourceStr = "DEBUG_SOURCE_API";	            break;
        case GL_DEBUG_SOURCE_WINDOW_SYSTEM:	    sourceStr = "DEBUG_SOURCE_WINDOW_SYSTEM";	break;
        case GL_DEBUG_SOURCE_SHADER_COMPILER:	sourceStr = "DEBUG_SOURCE_SHADER_COMPILER";	break;
        case GL_DEBUG_SOURCE_THIRD_PARTY:	    sourceStr = "DEBUG_SOURCE_THIRD_PARTY";	    break;
        case GL_DEBUG_SOURCE_APPLICATION:	    sourceStr = "DEBUG_SOURCE_APPLICATION";	    break;
        case GL_DEBUG_SOURCE_OTHER:	            sourceStr = "DEBUG_SOURCE_OTHER";	        break;
        default:	                            sourceStr = "UNKNOWN";	                    break;
    }

    const char *typeStr;
    switch( type )
    {
        case GL_DEBUG_TYPE_ERROR:               typeStr = "DEBUG_TYPE_ERROR";	            break;
        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: typeStr = "DEBUG_TYPE_DEPRECATED_BEHAVIOR";	break;
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  typeStr = "DEBUG_TYPE_UNDEFINED_BEHAVIOR";	break;
        case GL_DEBUG_TYPE_PORTABILITY:         typeStr = "DEBUG_TYPE_PORTABILITY";	        break;
        case GL_DEBUG_TYPE_PERFORMANCE:         typeStr = "DEBUG_TYPE_PERFORMANCE";	        break;
        case GL_DEBUG_TYPE_MARKER:              typeStr = "DEBUG_TYPE_MARKER";	            break;
        case GL_DEBUG_TYPE_PUSH_GROUP:          typeStr = "DEBUG_TYPE_PUSH_GROUP";	        break;
        case GL_DEBUG_TYPE_POP_GROUP:           typeStr = "DEBUG_TYPE_POP_GROUP";	        break;
        case GL_DEBUG_TYPE_OTHER:               typeStr = "DEBUG_TYPE_OTHER";	            break;
        default:	                            typeStr = "UNKNOWN";	                    break;
    }

    const char *severityStr;
    switch( severity )
    {
        case GL_DEBUG_SEVERITY_HIGH:	        severityStr = "DEBUG_SEVERITY_HIGH";	        break;
        case GL_DEBUG_SEVERITY_MEDIUM:	        severityStr = "DEBUG_SEVERITY_MEDIUM";	        break;
        case GL_DEBUG_SEVERITY_LOW:	            severityStr = "DEBUG_SEVERITY_LOW";	            break;
        case GL_DEBUG_SEVERITY_NOTIFICATION:	severityStr = "DEBUG_SEVERITY_NOTIFICATION";	break;
        default:	                            severityStr = "UNKNOWN";	                    break;
    }

    LOG( "OpenGL debug message:\n"
         "source\t\t:: %s\n"
         "type\t\t:: %s\n"
         "id\t\t\t:: %#X\n"
         "severity\t:: %s\n"
         "message\t\t:: %s\n",
         sourceStr, typeStr, id, severityStr, message );

    if( type != GL_DEBUG_TYPE_PERFORMANCE &&
        severity != GL_DEBUG_SEVERITY_NOTIFICATION )
    {
        ASSERT( !"OpenGL error" );
    }
}

internal OpenGLInfo
OpenGLGetInfo( bool modernContext )
{
    OpenGLInfo result = {};
    result.modernContext = modernContext;
    result.vendor = (const char *)glGetString( GL_VENDOR );
    result.renderer = (const char *)glGetString( GL_RENDERER );
    result.version = (const char *)glGetString( GL_VERSION );

    if( modernContext )
    {
        result.SLversion = (const char *)glGetString( GL_SHADING_LANGUAGE_VERSION );

        GLuint n, i;
        glGetIntegerv(GL_NUM_EXTENSIONS, (GLint*)&n);
        // FIXME Not safe
        ASSERT( n < ARRAYCOUNT(result.extensions) );

        for( i = 0; i < n; i++ )
        {
            result.extensions[i] = (char *)glGetStringi( GL_EXTENSIONS, i );
        }
    }
    else
    {
        char *extensionsString = (char *)glGetString( GL_EXTENSIONS );
        u32 extensionIndex = 0;

        char *nextToken = NULL;
        char *str = strtok_s( extensionsString, " ", &nextToken );
        while( str != NULL )
        {
            result.extensions[extensionIndex++] = str;
            // FIXME Not safe
            ASSERT( extensionIndex < ARRAYCOUNT(result.extensions) );

            str = strtok_s( NULL, " ", &nextToken );
        }
    }

    return result;
}

internal GLint
OpenGLCompileShader( GLenum shaderType, const char *shaderSource, GLuint *outShaderId, char const* displayName )
{
    LOG( ".Compiling shader '%s'..", displayName );

    GLuint shader = glCreateShader( shaderType );
    if( shader == 0 )
        return GL_FALSE;

    // FIXME
    glShaderSource( shader, 1, &shaderSource, NULL );     // That ** is super wrong!
    glCompileShader( shader );

    GLint success;
    char infoLog[1024];
    glGetShaderiv( shader, GL_COMPILE_STATUS, &success );

    if( success )
        *outShaderId = shader;
    else
    {
        glGetShaderInfoLog( shader, ARRAYCOUNT(infoLog), NULL, infoLog );
        LOG( ".ERROR :: Shader compilation failed!\n%s\n", infoLog );
    }

    return success;
}

internal GLint
OpenGLLinkProgram( OpenGLShaderProgram *prg )
{
    GLuint programId;

    // TODO Identify programs by a string instead of an index
    programId = glCreateProgram();
    glAttachShader( programId, prg->vsId );
    if( prg->gsId )
        glAttachShader( programId, prg->gsId );
    glAttachShader( programId, prg->fsId );

    // NOTE Explicitly bind indices so that we don't need to conditionally bind shit later
    // only to avoid stupid fucking errors
    for( u32 a = 0; a < ARRAYCOUNT(prg->attribs); ++a )
    {
        OpenGLShaderAttribute &attr = prg->attribs[a];
        if( attr.name )
            glBindAttribLocation( programId, a, attr.name );
    }

    GLint success;
    char infoLog[1024];
    glLinkProgram( programId );
    glGetProgramiv( programId, GL_LINK_STATUS, &success );
    if( !success )
    {
        glGetProgramInfoLog( programId, ARRAYCOUNT(infoLog), NULL, infoLog );
        LOG( ".ERROR :: Shader program linkage failed!\n%s\n", infoLog );
        return success;
    }

    // Check attribute & uniform locations
    for( int u = 0; u < ARRAYCOUNT(prg->uniforms); ++u )
    {
        OpenGLShaderUniform &uniform = prg->uniforms[u];
        if( uniform.name )
        {
            uniform.locationId = glGetUniformLocation( programId, uniform.name );
            if( uniform.locationId == -1 )
                LOG( ".WARNING :: Uniform '%s' is not active in shader program %d", uniform.name, prg->name );
        }
    }

    for( int a = 0; a < ARRAYCOUNT(prg->attribs); ++a )
    {
        OpenGLShaderAttribute &attr = prg->attribs[a];
        if( attr.name )
        {
            if( glGetAttribLocation( programId, attr.name ) == -1 )
                LOG( ".WARNING :: Attribute '%s' is not active in shader program %d", attr.name, prg->name );
        }
    }

    prg->programId = programId;
    return success;
}

internal 
DEBUG_GAME_ASSET_LOADED_CALLBACK(OpenGLHotswapShader)
{
    const char* shaderSource = (const char*)readFile.contents;

    // Find which program definition(s) the file belongs to
    for( int i = 0; i < ARRAYCOUNT(globalShaderPrograms); ++i )
    {
        OpenGLShaderProgram &prg = globalShaderPrograms[i];

        bool found = true;
        GLuint newId;
        GLint success = GL_FALSE;

        if( strcmp( filename, prg.vsFilename ) == 0 )
        {
            success = OpenGLCompileShader( GL_VERTEX_SHADER, shaderSource, &newId, prg.vsFilename );
            if( success )
            {
                prg.vsId = newId;
                prg.vsSource = shaderSource;
            }
        }
        else if( prg.gsFilename && strcmp( filename, prg.gsFilename ) == 0 )
        {
            success = OpenGLCompileShader( GL_GEOMETRY_SHADER, shaderSource, &newId, prg.gsFilename );
            if( success )
            {
                prg.gsId = newId;
                prg.gsSource = shaderSource;
            }
        }
        else if( strcmp( filename, prg.fsFilename ) == 0 )
        {
            success = OpenGLCompileShader( GL_FRAGMENT_SHADER, shaderSource, &newId, prg.fsFilename );
            if( success )
            {
                prg.fsId = newId;
                prg.fsSource = shaderSource;
            }
        }
        else
            found = false;

        if( found && success )
            success = OpenGLLinkProgram( &prg );

        // TODO Log failures?
    }
}

internal
PLATFORM_ALLOCATE_OR_UPDATE_TEXTURE(OpenGLAllocateTexture)
{
    void* result = nullptr;

    GLuint textureHandle;
    if( optionalHandle )
        textureHandle = (GLuint)(sz)optionalHandle;
    else
        glGenTextures( 1, &textureHandle );

    glBindTexture( GL_TEXTURE_2D, textureHandle );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
    // Use GL_LINEAR_MIPMAP_LINEAR?
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filtered ? GL_LINEAR : GL_NEAREST );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filtered ? GL_LINEAR : GL_NEAREST );

    if( optionalHandle )
        glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, data );
    else
        glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data );

    glGenerateMipmap( GL_TEXTURE_2D );

    result = (void*)textureHandle;
    return result;
}

internal
PLATFORM_DEALLOCATE_TEXTURE(OpenGLDeallocateTexture)
{
    // TODO 
}

OpenGLInfo
OpenGLInit( OpenGLState &gl, bool modernContext )
{
    OpenGLInfo info = OpenGLGetInfo( modernContext );

#if !RELEASE
    if( glDebugMessageCallbackARB )
    {
        glEnable( GL_DEBUG_OUTPUT_SYNCHRONOUS );
        glDebugMessageCallbackARB( OpenGLDebugCallback, 0 );
    }
#endif

#if 1
    // According to Muratori, changing VAOs is inefficient and not really used anymore
    // (gotta ask him about that claim..)
    GLuint dummyVAO;
    glGenVertexArrays( 1, &dummyVAO );
    glBindVertexArray( dummyVAO );
#endif

    // We're also using a single VBO and index buffer
    // TODO Test performance when using just one vs various
    glGenBuffers( 1, &gl.vertexBuffer );
    glGenBuffers( 1, &gl.indexBuffer );
    glGenBuffers( 1, &gl.instanceBuffer );

    // Compile all program definitions
    for( int i = 0; i < ARRAYCOUNT(globalShaderPrograms); ++i )
    {
        OpenGLShaderProgram &prg = globalShaderPrograms[i];
        //LOG( ".Compiling program '%s'..", prg.name );

        char filePath[MAX_PATH];
        DEBUGReadFileResult result;
        GLint success;

        // Vertex shader
        // TODO Do automatic asset resolution (including relative paths) in the platform, something like:
        //result = globalPlatform.LoadAsset( PlatformAsset::SHADER, prg.vsFilename );
        strcpy( filePath, SHADERS_RELATIVE_PATH );
        strcat( filePath, prg.vsFilename );
        result = globalPlatform.DEBUGReadEntireFile( filePath );
        ASSERT( result.contents );
        prg.vsSource = (char *)result.contents;

        success = OpenGLCompileShader( GL_VERTEX_SHADER, prg.vsSource, &prg.vsId, prg.vsFilename );
        globalPlatform.DEBUGFreeFileMemory( result.contents );
        if( !success )
            continue;

        if( prg.gsFilename )
        {
            // Geometry shader
            strcpy( filePath, SHADERS_RELATIVE_PATH );
            strcat( filePath, prg.gsFilename );
            result = globalPlatform.DEBUGReadEntireFile( filePath );
            ASSERT( result.contents );
            prg.gsSource = (char *)result.contents;

            success = OpenGLCompileShader( GL_GEOMETRY_SHADER, prg.gsSource, &prg.gsId, prg.gsFilename );
            globalPlatform.DEBUGFreeFileMemory( result.contents );
            if( !success )
                continue;
        }

        // Fragment shader
        strcpy( filePath, SHADERS_RELATIVE_PATH );
        strcat( filePath, prg.fsFilename );
        result = globalPlatform.DEBUGReadEntireFile( filePath );
        ASSERT( result.contents );
        prg.fsSource = (char *)result.contents;

        success = OpenGLCompileShader( GL_FRAGMENT_SHADER, prg.fsSource, &prg.fsId, prg.fsFilename );
        globalPlatform.DEBUGFreeFileMemory( result.contents );
        if( !success )
            continue;

        // Create and link program
        success = OpenGLLinkProgram( &prg );
        
    }

    // Create a white texture
    gl.white = 0xFFFFFFFF;
    gl.whiteTexture = OpenGLAllocateTexture( (u8*)&gl.white, 1, 1, true, nullptr );

    ASSERT_GL_STATE;
    return info;
}

internal void
OpenGLRenderImGui( const OpenGLState& gl, ImDrawData *drawData )
{
    // Avoid rendering when minimized, scale coordinates for retina displays (screen coordinates != framebuffer coordinates)
    ImGuiIO& io = ImGui::GetIO();
    int fbWidth = (int)(io.DisplaySize.x * io.DisplayFramebufferScale.x);
    int fbHeight = (int)(io.DisplaySize.y * io.DisplayFramebufferScale.y);
    if (fbWidth == 0 || fbHeight == 0)
        return;
    drawData->ScaleClipRects( io.DisplayFramebufferScale );

    // Setup render state: alpha-blending enabled, no face culling, no depth testing, scissor enabled
    glActiveTexture( GL_TEXTURE0 );
    glEnable( GL_BLEND );
    glBlendEquation( GL_FUNC_ADD );
    glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
    glDisable( GL_CULL_FACE );
    glDisable( GL_DEPTH_TEST );
    glEnable( GL_SCISSOR_TEST );
    glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );

    // Setup viewport, orthographic projection matrix
    glViewport( 0, 0, (GLsizei)fbWidth, (GLsizei)fbHeight );
    m4 orthoProj = M4Orthographic( io.DisplaySize.x, io.DisplaySize.y );
    glUseProgram( gl.imGui.program );
    glUniform1i( gl.imGui.texUniformId, 0 );
    glUniformMatrix4fv( gl.imGui.projUniformId, 1, GL_FALSE, orthoProj.e[0] );

    // TODO This switching of formats every frame probably means we should consider a
    // separate VAO for UI & game
    glEnableVertexAttribArray( gl.imGui.pAttribId );
    glEnableVertexAttribArray( gl.imGui.uvAttribId );
    glEnableVertexAttribArray( gl.imGui.cAttribId );

    glVertexAttribPointer( gl.imGui.pAttribId, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (GLvoid*)OFFSETOF(ImDrawVert, pos) );
    glVertexAttribPointer( gl.imGui.uvAttribId, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (GLvoid*)OFFSETOF(ImDrawVert, uv) );
    glVertexAttribPointer( gl.imGui.cAttribId, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(ImDrawVert), (GLvoid*)OFFSETOF(ImDrawVert, col) );

    for( int cmdListIdx = 0; cmdListIdx < drawData->CmdListsCount; cmdListIdx++ )
    {
        const ImDrawList* cmdList = drawData->CmdLists[cmdListIdx];
        const ImDrawIdx* indexBufferOffset = 0;

        glBufferData( GL_ARRAY_BUFFER, (GLsizeiptr)cmdList->VtxBuffer.Size * SIZE(ImDrawVert), (const GLvoid*)cmdList->VtxBuffer.Data, GL_STREAM_DRAW );
        glBufferData( GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)cmdList->IdxBuffer.Size * SIZE(ImDrawIdx), (const GLvoid*)cmdList->IdxBuffer.Data, GL_STREAM_DRAW );

        for( int cmdIdx = 0; cmdIdx < cmdList->CmdBuffer.Size; cmdIdx++ )
        {
            const ImDrawCmd* cmd = &cmdList->CmdBuffer[cmdIdx];
            if( cmd->UserCallback )
            {
                cmd->UserCallback( cmdList, cmd );
            }
            else
            {
                glBindTexture( GL_TEXTURE_2D, (GLuint)(intptr_t)cmd->TextureId );
                glScissor( (int)cmd->ClipRect.x, (int)(fbHeight - cmd->ClipRect.w), (int)(cmd->ClipRect.z - cmd->ClipRect.x), (int)(cmd->ClipRect.w - cmd->ClipRect.y) );
                glDrawElements( GL_TRIANGLES, (GLsizei)cmd->ElemCount, sizeof(ImDrawIdx) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT, indexBufferOffset );
            }
            indexBufferOffset += cmd->ElemCount;
        }
    }
}

inline void SetupImGuiStyle( bool bStyleDark_, float alpha_  )
{
    ImGuiStyle& style = ImGui::GetStyle();

#if 0
    // light style from Pacôme Danhiez (user itamago) https://github.com/ocornut/imgui/pull/511#issuecomment-175719267
    style.Alpha = 1.0f;
    style.FrameRounding = 3.0f;
    style.Colors[ImGuiCol_Text]                  = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_TextDisabled]          = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
    style.Colors[ImGuiCol_WindowBg]              = ImVec4(0.94f, 0.94f, 0.94f, 0.94f);
    style.Colors[ImGuiCol_ChildWindowBg]         = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    style.Colors[ImGuiCol_PopupBg]               = ImVec4(1.00f, 1.00f, 1.00f, 0.94f);
    style.Colors[ImGuiCol_Border]                = ImVec4(0.00f, 0.00f, 0.00f, 0.39f);
    style.Colors[ImGuiCol_BorderShadow]          = ImVec4(1.00f, 1.00f, 1.00f, 0.10f);
    style.Colors[ImGuiCol_FrameBg]               = ImVec4(1.00f, 1.00f, 1.00f, 0.94f);
    style.Colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.26f, 0.59f, 0.98f, 0.40f);
    style.Colors[ImGuiCol_FrameBgActive]         = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
    style.Colors[ImGuiCol_TitleBg]               = ImVec4(0.96f, 0.96f, 0.96f, 1.00f);
    style.Colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(1.00f, 1.00f, 1.00f, 0.51f);
    style.Colors[ImGuiCol_TitleBgActive]         = ImVec4(0.82f, 0.82f, 0.82f, 1.00f);
    style.Colors[ImGuiCol_MenuBarBg]             = ImVec4(0.86f, 0.86f, 0.86f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.98f, 0.98f, 0.98f, 0.53f);
    style.Colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.69f, 0.69f, 0.69f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.59f, 0.59f, 0.59f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.49f, 0.49f, 0.49f, 1.00f);
    style.Colors[ImGuiCol_ComboBg]               = ImVec4(0.86f, 0.86f, 0.86f, 0.99f);
    style.Colors[ImGuiCol_CheckMark]             = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    style.Colors[ImGuiCol_SliderGrab]            = ImVec4(0.24f, 0.52f, 0.88f, 1.00f);
    style.Colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    style.Colors[ImGuiCol_Button]                = ImVec4(0.26f, 0.59f, 0.98f, 0.40f);
    style.Colors[ImGuiCol_ButtonHovered]         = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    style.Colors[ImGuiCol_ButtonActive]          = ImVec4(0.06f, 0.53f, 0.98f, 1.00f);
    style.Colors[ImGuiCol_Header]                = ImVec4(0.26f, 0.59f, 0.98f, 0.31f);
    style.Colors[ImGuiCol_HeaderHovered]         = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
    style.Colors[ImGuiCol_HeaderActive]          = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    style.Colors[ImGuiCol_Separator]                = ImVec4(0.39f, 0.39f, 0.39f, 1.00f);
    style.Colors[ImGuiCol_SeparatorHovered]         = ImVec4(0.26f, 0.59f, 0.98f, 0.78f);
    style.Colors[ImGuiCol_SeparatorActive]          = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    style.Colors[ImGuiCol_ResizeGrip]            = ImVec4(1.00f, 1.00f, 1.00f, 0.50f);
    style.Colors[ImGuiCol_ResizeGripHovered]     = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
    style.Colors[ImGuiCol_ResizeGripActive]      = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
    style.Colors[ImGuiCol_CloseButton]           = ImVec4(0.59f, 0.59f, 0.59f, 0.50f);
    style.Colors[ImGuiCol_CloseButtonHovered]    = ImVec4(0.98f, 0.39f, 0.36f, 1.00f);
    style.Colors[ImGuiCol_CloseButtonActive]     = ImVec4(0.98f, 0.39f, 0.36f, 1.00f);
    style.Colors[ImGuiCol_PlotLines]             = ImVec4(0.39f, 0.39f, 0.39f, 1.00f);
    style.Colors[ImGuiCol_PlotLinesHovered]      = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
    style.Colors[ImGuiCol_PlotHistogram]         = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_PlotHistogramHovered]  = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_TextSelectedBg]        = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
    style.Colors[ImGuiCol_ModalWindowDarkening]  = ImVec4(0.20f, 0.20f, 0.20f, 0.35f);

    /*
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 5.3f;
    style.FrameRounding = 2.3f;
    style.ScrollbarRounding = 0;

    style.Colors[ImGuiCol_Text]                  = ImVec4(0.90f, 0.90f, 0.90f, 0.90f);
    style.Colors[ImGuiCol_TextDisabled]          = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
    style.Colors[ImGuiCol_WindowBg]              = ImVec4(0.09f, 0.09f, 0.15f, 1.00f);
    style.Colors[ImGuiCol_ChildWindowBg]         = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    style.Colors[ImGuiCol_PopupBg]               = ImVec4(0.05f, 0.05f, 0.10f, 0.85f);
    style.Colors[ImGuiCol_Border]                = ImVec4(0.70f, 0.70f, 0.70f, 0.65f);
    style.Colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    style.Colors[ImGuiCol_FrameBg]               = ImVec4(0.00f, 0.00f, 0.01f, 1.00f);
    style.Colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.90f, 0.80f, 0.80f, 0.40f);
    style.Colors[ImGuiCol_FrameBgActive]         = ImVec4(0.90f, 0.65f, 0.65f, 0.45f);
    style.Colors[ImGuiCol_TitleBg]               = ImVec4(0.00f, 0.00f, 0.00f, 0.83f);
    style.Colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.40f, 0.40f, 0.80f, 0.20f);
    style.Colors[ImGuiCol_TitleBgActive]         = ImVec4(0.00f, 0.00f, 0.00f, 0.87f);
    style.Colors[ImGuiCol_MenuBarBg]             = ImVec4(0.01f, 0.01f, 0.02f, 0.80f);
    style.Colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.20f, 0.25f, 0.30f, 0.60f);
    style.Colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.55f, 0.53f, 0.55f, 0.51f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.56f, 0.56f, 0.56f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.56f, 0.56f, 0.56f, 0.91f);
    style.Colors[ImGuiCol_ComboBg]               = ImVec4(0.1f, 0.1f, 0.1f, 0.99f);
    style.Colors[ImGuiCol_CheckMark]             = ImVec4(0.90f, 0.90f, 0.90f, 0.83f);
    style.Colors[ImGuiCol_SliderGrab]            = ImVec4(0.70f, 0.70f, 0.70f, 0.62f);
    style.Colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.30f, 0.30f, 0.30f, 0.84f);
    style.Colors[ImGuiCol_Button]                = ImVec4(0.48f, 0.72f, 0.89f, 0.49f);
    style.Colors[ImGuiCol_ButtonHovered]         = ImVec4(0.50f, 0.69f, 0.99f, 0.68f);
    style.Colors[ImGuiCol_ButtonActive]          = ImVec4(0.80f, 0.50f, 0.50f, 1.00f);
    style.Colors[ImGuiCol_Header]                = ImVec4(0.30f, 0.69f, 1.00f, 0.53f);
    style.Colors[ImGuiCol_HeaderHovered]         = ImVec4(0.44f, 0.61f, 0.86f, 1.00f);
    style.Colors[ImGuiCol_HeaderActive]          = ImVec4(0.38f, 0.62f, 0.83f, 1.00f);
    style.Colors[ImGuiCol_Separator]             = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    style.Colors[ImGuiCol_SeparatorHovered]      = ImVec4(0.70f, 0.60f, 0.60f, 1.00f);
    style.Colors[ImGuiCol_SeparatorActive]       = ImVec4(0.90f, 0.70f, 0.70f, 1.00f);
    style.Colors[ImGuiCol_ResizeGrip]            = ImVec4(1.00f, 1.00f, 1.00f, 0.85f);
    style.Colors[ImGuiCol_ResizeGripHovered]     = ImVec4(1.00f, 1.00f, 1.00f, 0.60f);
    style.Colors[ImGuiCol_ResizeGripActive]      = ImVec4(1.00f, 1.00f, 1.00f, 0.90f);
    style.Colors[ImGuiCol_CloseButton]           = ImVec4(0.50f, 0.50f, 0.90f, 0.50f);
    style.Colors[ImGuiCol_CloseButtonHovered]    = ImVec4(0.70f, 0.70f, 0.90f, 0.60f);
    style.Colors[ImGuiCol_CloseButtonActive]     = ImVec4(0.70f, 0.70f, 0.70f, 1.00f);
    style.Colors[ImGuiCol_PlotLines]             = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    style.Colors[ImGuiCol_PlotLinesHovered]      = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_PlotHistogram]         = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_PlotHistogramHovered]  = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_TextSelectedBg]        = ImVec4(0.00f, 0.00f, 1.00f, 0.35f);
    style.Colors[ImGuiCol_ModalWindowDarkening]  = ImVec4(0.20f, 0.20f, 0.20f, 0.35f);
    */

    if( bStyleDark_ )
    {
        for (int i = 0; i <= ImGuiCol_COUNT; i++)
        {
            ImVec4& col = style.Colors[i];
            float H, S, V;
            ImGui::ColorConvertRGBtoHSV( col.x, col.y, col.z, H, S, V );

            if( S < 0.1f )
            {
                V = 1.0f - V;
            }
            ImGui::ColorConvertHSVtoRGB( H, S, V, col.x, col.y, col.z );
            if( col.w < 1.00f )
            {
                col.w *= alpha_;
            }
        }
    }
    else
    {
        for (int i = 0; i <= ImGuiCol_COUNT; i++)
        {
            ImVec4& col = style.Colors[i];
            if( col.w < 1.00f )
            {
                col.x *= alpha_;
                col.y *= alpha_;
                col.z *= alpha_;
                col.w *= alpha_;
            }
        }
    }
#endif
}

ImGuiContext *
OpenGLInitImGui( OpenGLState &gl )
{
    LOG( ".Initializing ImGui version %s", ImGui::GetVersion() );

    // FIXME Create an arena (when we have dynamic arenas) for ImGui and pass a custom allocator/free here (and not use new for the atlas!)
    ImGuiContext *context = ImGui::CreateContext();
    ImGui::SetCurrentContext( context );
    ImGui::GetIO().Fonts = new ImFontAtlas();

    const GLchar *vertShader =
        "#version 330\n"
        "uniform mat4 ProjMtx;\n"
        "in vec2 Position;\n"
        "in vec2 UV;\n"
        "in vec4 Color;\n"
        "out vec2 Frag_UV;\n"
        "out vec4 Frag_Color;\n"
        "void main()\n"
        "{\n"
        "	Frag_UV = UV;\n"
        "	Frag_Color = Color;\n"
        "	gl_Position = ProjMtx * vec4(Position.xy,0,1);\n"
        "}\n";

    const GLchar* fragShader =
        "#version 330\n"
        "uniform sampler2D Texture;\n"
        "in vec2 Frag_UV;\n"
        "in vec4 Frag_Color;\n"
        "out vec4 Out_Color;\n"
        "void main()\n"
        "{\n"
        "	Out_Color = Frag_Color * texture( Texture, Frag_UV.st);\n"
        "}\n";

    gl.imGui.program = glCreateProgram();
    gl.imGui.vertShader = glCreateShader( GL_VERTEX_SHADER );
    gl.imGui.fragShader = glCreateShader( GL_FRAGMENT_SHADER );
    glShaderSource( gl.imGui.vertShader, 1, &vertShader, 0 );
    glShaderSource( gl.imGui.fragShader, 1, &fragShader, 0 );
    glCompileShader( gl.imGui.vertShader );
    glCompileShader( gl.imGui.fragShader );
    glAttachShader( gl.imGui.program, gl.imGui.vertShader );
    glAttachShader( gl.imGui.program, gl.imGui.fragShader );
    glLinkProgram( gl.imGui.program );

    gl.imGui.texUniformId   = glGetUniformLocation( gl.imGui.program, "Texture" );
    gl.imGui.projUniformId  = glGetUniformLocation( gl.imGui.program, "ProjMtx" );
    gl.imGui.pAttribId	    = U32( glGetAttribLocation( gl.imGui.program, "Position" ) );
    gl.imGui.uvAttribId	    = U32( glGetAttribLocation( gl.imGui.program, "UV" ) );
    gl.imGui.cAttribId	    = U32( glGetAttribLocation( gl.imGui.program, "Color" ) );

    // Build texture atlas
    ImGuiIO &io = ImGui::GetIO();
    u8 *pixels;
    i32 width, height;
    // Load as RGBA 32-bits (75% of the memory is wasted, but default font is so small)
    // because it is more likely to be compatible with user's existing shaders.
    // If your ImTextureId represent a higher-level concept than just a GL texture id,
    // consider calling GetTexDataAsAlpha8() instead to save on GPU memory.
    io.Fonts->GetTexDataAsRGBA32( &pixels, &width, &height );

    // Upload texture to graphics system
    glGenTextures( 1, &gl.imGui.fontTexture );
    glBindTexture( GL_TEXTURE_2D, gl.imGui.fontTexture );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
    glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels );

    ASSERT_GL_STATE;

    // Store our identifier
    io.Fonts->TexID = (void *)(intptr_t)gl.imGui.fontTexture;

    // TODO Override memory allocation functions!

    //SetupImGuiStyle( true, 0.98f );

    return context;
}

internal void
OpenGLUseProgram( ShaderProgramName programName, RenderCommands const& commands, OpenGLState* gl )
{
    if( programName == ShaderProgramName::None )
    {
        for( u32 i = 0; i < MAX_SHADER_ATTRIBS; ++i )
            glDisableVertexAttribArray( i );

        glUseProgram( 0 );

        gl->activeProgram = nullptr;
    }
    else
    {
        if( gl->activeProgram && gl->activeProgram->name == programName )
            // Nothing to do
            return;

        int programIndex = -1;
        for( int i = 0; i < ARRAYCOUNT(globalShaderPrograms); ++i )
        {
            if( globalShaderPrograms[i].name == programName )
            {
                programIndex = i;
                break;
            }
        }

        if( programIndex == -1 )
        {
            LOG( "ERROR :: Unknown shader program [%d]", programName );
        }
        else
        {
            OpenGLShaderProgram &prg = globalShaderPrograms[programIndex];
            glUseProgram( prg.programId );

            // FIXME This exact procedure will of course vary for different programs,
            // but I want to see exactly how before doing anything more abstracted

            // Material *matPtr = entry->materialArray;

            glBindBuffer( GL_ARRAY_BUFFER, gl->vertexBuffer );

            // mTransform
            glUniformMatrix4fv( prg.uniforms[0].locationId, 1, GL_TRUE, gl->currentProjectViewM.e[0] );
            // simClusterOffsets
            glUniform3fv( prg.uniforms[1].locationId, commands.simClusterCount, (GLfloat*)commands.simClusterOffsets );
            // simClusterIndex
            glUniform1ui( prg.uniforms[2].locationId, 0 );

            GLuint pAttribId = 0;
            GLuint uvAttribId = 1;
            GLuint cAttribId = 2;

            glEnableVertexAttribArray( pAttribId );
            glEnableVertexAttribArray( uvAttribId );
            glEnableVertexAttribArray( cAttribId );

            // inPosition
            glVertexAttribPointer( pAttribId, 3, GL_FLOAT, false, sizeof(TexturedVertex), (void *)OFFSETOF(TexturedVertex, p) );
            // inTexCoords
            glVertexAttribPointer( uvAttribId, 2, GL_FLOAT, false, sizeof(TexturedVertex), (void *)OFFSETOF(TexturedVertex, uv) );
            // inColor
            // NOTE glVertexAttribPointer cannot be used with integral data. Beware the I!!!
            glVertexAttribIPointer( cAttribId, 1, GL_UNSIGNED_INT, sizeof(TexturedVertex), (void *)OFFSETOF(TexturedVertex, color) );

            gl->activeProgram = &prg;
        }
    }
}

internal void
OpenGLNewFrame( const RenderCommands &commands, OpenGLState* gl )
{
    int viewportWidth = commands.width;
    int viewportHeight = commands.height;
    glViewport( 0, 0, viewportWidth, viewportHeight );
    
    glDisable( GL_SCISSOR_TEST );
    glEnable( GL_CULL_FACE );
    glEnable( GL_DEPTH_TEST );
    glFrontFace( GL_CCW );

    // TODO Create a 1x1 white texture to bind here so that shaders that use samplers
    // don't have to check whether there's a 'valid' texture or not
    glBindTexture( GL_TEXTURE_2D, 0 );

    gl->currentProjectViewM = commands.camera.projectFromWorld;
}

internal void
OpenGLRenderToOutput( const RenderCommands &commands, OpenGLState* gl, GameMemory* gameMemory )
{
    OpenGLNewFrame( commands, gl );

#if !RELEASE
    if( gl->queryObjectId == 0 )
        glGenQueries( 1, &gl->queryObjectId );

    // Ask the GPU how many extra primitives were generated in shaders
    glBeginQuery( GL_PRIMITIVES_GENERATED, gl->queryObjectId );
#endif

    u32 totalDrawCalls = 0;
    u32 totalVertexCount = 0;
    u32 totalPrimitiveCount = 0;
    u32 totalInstanceCount = 0;
    u32 totalMeshCount = 0;

    const RenderBuffer &buffer = commands.renderBuffer;
    for( int baseAddress = 0; baseAddress < buffer.size; /**/ )
    {
        RenderEntry *entryHeader = (RenderEntry *)(buffer.base + baseAddress);

        switch( entryHeader->type )
        {
            case RenderEntryType::RenderEntryClear:
            {
                RenderEntryClear *entry = (RenderEntryClear *)entryHeader;

                glClearColor( entry->color.r, entry->color.g, entry->color.b, entry->color.a ); 
                glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
            } break;

            case RenderEntryType::RenderEntryTexturedTris:
            {
                // TODO Try out some of the approaches mentioned in http://www.seas.upenn.edu/~pcozzi/OpenGLInsights/OpenGLInsights-AsynchronousBufferTransfers.pdf
                // (specially example 28.4 - unsynchronized mapping - seems to be the most performant)
                // Why, maybe even be super scientific about it and implement the various paths with a key that can switch among them for A/B testing
                //
                // Also, weigh whether we may want to track mesh changes in our buffers so that buffer data doesn't have to be resubmitted for more static stuff
                // (that all will depend on whether we really end up having much static or not, but maybe for the 'hull chunks' it could be worth it?

                RenderEntryTexturedTris *entry = (RenderEntryTexturedTris *)entryHeader;

                GLuint vertexByteCount = entry->vertexCount * sizeof(TexturedVertex);
                GLuint indexByteCount = entry->indexCount * sizeof(u32);

                glBindBuffer( GL_ARRAY_BUFFER, gl->vertexBuffer );
                glBufferData( GL_ARRAY_BUFFER,
                              vertexByteCount,
                              commands.vertexBuffer.base + entry->vertexBufferOffset,
                              GL_STREAM_DRAW );
                glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, gl->indexBuffer );
                glBufferData( GL_ELEMENT_ARRAY_BUFFER,
                              indexByteCount,
                              commands.indexBuffer.base + entry->indexBufferOffset,
                              GL_STREAM_DRAW );

                glDrawElements( GL_TRIANGLES, entry->indexCount, GL_UNSIGNED_INT, (void *)0 );

                totalDrawCalls++;
                totalVertexCount += entry->vertexCount;
                totalPrimitiveCount += (entry->indexCount / 3);
            } break;

            case RenderEntryType::RenderEntryLines:
            {
                RenderEntryLines *entry = (RenderEntryLines *)entryHeader;

                GLuint countBytes = entry->lineCount * 2 * sizeof(TexturedVertex);
                glBindBuffer( GL_ARRAY_BUFFER, gl->vertexBuffer );
                glBufferData( GL_ARRAY_BUFFER,
                              countBytes,
                              commands.vertexBuffer.base + entry->vertexBufferOffset,
                              GL_STREAM_DRAW );

                glDrawArrays( GL_LINES, 0, entry->lineCount * 2 );

                totalDrawCalls++;
                totalVertexCount += entry->lineCount * 2;
                totalPrimitiveCount += entry->lineCount;
            } break;

            case RenderEntryType::RenderEntryProgramChange:
            {
                RenderEntryProgramChange* entry = (RenderEntryProgramChange*)entryHeader;

                OpenGLUseProgram( entry->programName, commands, gl );
            } break;

            case RenderEntryType::RenderEntryMaterial:
            {
                RenderEntryMaterial* entry = (RenderEntryMaterial*)entryHeader;
                Material* material = entry->material;

                GLuint materialId = (GLuint)(sz)(material ? material->diffuseMap : gl->whiteTexture);
                glBindTexture( GL_TEXTURE_2D, materialId );
            } break;

            case RenderEntryType::RenderEntrySwitch:
            {
                RenderEntrySwitch* entry = (RenderEntrySwitch*)entryHeader;
                bool enable = entry->enable;

                switch( entry->renderSwitch )
                {
                    case RenderSwitchType::Culling:
                        if( enable )
                            glEnable( GL_CULL_FACE );
                        else
                            glDisable( GL_CULL_FACE );
                        break;
                        
                    INVALID_DEFAULT_CASE
                }
            } break;

            case RenderEntryType::RenderEntryVoxelGrid:
            {
                RenderEntryVoxelGrid* entry = (RenderEntryVoxelGrid*)entryHeader;

                const u32 lineCount = 12;
                const GLuint countBytes = lineCount * 2 * sizeof(TexturedVertex);

                // TODO Should be part of the shader setup
                glBindBuffer( GL_ARRAY_BUFFER, gl->instanceBuffer );
                glBufferData( GL_ARRAY_BUFFER, SIZE(InstanceData) * entry->instanceCount,
                              commands.instanceBuffer.base + entry->instanceBufferOffset, GL_STATIC_DRAW );

                const GLuint offAttribId = 3;
                glEnableVertexAttribArray( offAttribId );
                // inInstanceOffset
                glVertexAttribPointer( offAttribId, 3, GL_FLOAT, false, sizeof(InstanceData), (void *)OFFSETOF(InstanceData, worldOffset) );
                // Update per instance
                glVertexAttribDivisor( offAttribId, 1 );

                const GLuint instColorAttribId = 4;
                glEnableVertexAttribArray( instColorAttribId );
                // inInstanceColor
                glVertexAttribIPointer( instColorAttribId, 1, GL_UNSIGNED_INT, sizeof(InstanceData), (void *)OFFSETOF(InstanceData, color) );
                // Update per instance
                glVertexAttribDivisor( instColorAttribId, 1 );

                glBindBuffer( GL_ARRAY_BUFFER, gl->vertexBuffer );
                glBufferData( GL_ARRAY_BUFFER,
                              countBytes,
                              commands.vertexBuffer.base + entry->vertexBufferOffset,
                              GL_STATIC_DRAW );
                glDrawArraysInstanced( GL_LINES, 0, lineCount * 2, entry->instanceCount );

                totalDrawCalls++;
                totalVertexCount += lineCount * 2;
                totalPrimitiveCount += lineCount;
                totalInstanceCount += entry->instanceCount;
            } break;

            case RenderEntryType::RenderEntryVoxelChunk:
            {
                RenderEntryVoxelChunk* entry = (RenderEntryVoxelChunk*)entryHeader;

                // TODO Should be part of the shader setup
                // TODO Should be part of the shader setup
                // TODO Should be part of the shader setup
                // TODO Should be part of the shader setup
                // Each shader should have a set of buffer endpoints (GLuint), each with a set of attribute definitions
                // After enabling a certain program, we need a call that provides the data for each buffer endpoint
                glBindBuffer( GL_ARRAY_BUFFER, gl->instanceBuffer );
                glBufferData( GL_ARRAY_BUFFER, SIZE(InstanceData) * entry->instanceCount,
                              commands.instanceBuffer.base + entry->instanceBufferOffset, GL_STATIC_DRAW );

                const GLuint offAttribId = 3;
                glEnableVertexAttribArray( offAttribId );
                // inInstanceOffset
                glVertexAttribPointer( offAttribId, 3, GL_FLOAT, false, sizeof(InstanceData), (void *)OFFSETOF(InstanceData, worldOffset) );
                // Update per instance
                glVertexAttribDivisor( offAttribId, 1 );

                const GLuint instColorAttribId = 4;
                glEnableVertexAttribArray( instColorAttribId );
                // inInstanceColor
                glVertexAttribIPointer( instColorAttribId, 1, GL_UNSIGNED_INT, sizeof(InstanceData), (void *)OFFSETOF(InstanceData, color) );
                // Update per instance
                glVertexAttribDivisor( instColorAttribId, 1 );

                GLuint vertexByteCount = 8 * sizeof(TexturedVertex);
                GLuint indexByteCount = 36 * sizeof(u32);

                glBindBuffer( GL_ARRAY_BUFFER, gl->vertexBuffer );
                glBufferData( GL_ARRAY_BUFFER,
                              vertexByteCount,
                              commands.vertexBuffer.base + entry->vertexBufferOffset,
                              GL_STATIC_DRAW );
                glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, gl->indexBuffer );
                glBufferData( GL_ELEMENT_ARRAY_BUFFER,
                              indexByteCount,
                              commands.indexBuffer.base + entry->indexBufferOffset,
                              GL_STATIC_DRAW );

                glDrawElementsInstanced( GL_TRIANGLES, 36, GL_UNSIGNED_INT, (void *)0, entry->instanceCount );

                totalDrawCalls++;
                totalVertexCount += 8;
                totalPrimitiveCount += 12;
                totalInstanceCount += entry->instanceCount;
            } break;

            case RenderEntryType::RenderEntryMeshChunk:
            {
                RenderEntryMeshChunk* entry = (RenderEntryMeshChunk*)entryHeader;

                glBindBuffer( GL_ARRAY_BUFFER, gl->vertexBuffer );
                glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, gl->indexBuffer );

                u32 runningVertexCount = 0;
                u32 runningIndexCount = 0;

                // TODO We should be able to send the whole buffer in one go and just change the cluster index uniform on each drawcall
                MeshData* mesh_data = (MeshData*)(commands.instanceBuffer.base + entry->instanceBufferOffset);
                for( int i = 0; i < entry->meshCount; ++i )
                {
                    GLuint vertexByteCount = mesh_data->vertexCount * sizeof(TexturedVertex);
                    GLuint indexByteCount = mesh_data->indexCount * sizeof(u32);

                    // simClusterIndex
                    glUniform1ui( gl->activeProgram->uniforms[2].locationId, U32( mesh_data->simClusterIndex ) );

                    glBufferData( GL_ARRAY_BUFFER,
                                  vertexByteCount,
                                  commands.vertexBuffer.base + entry->vertexBufferOffset + runningVertexCount,
                                  GL_STATIC_DRAW );
                    glBufferData( GL_ELEMENT_ARRAY_BUFFER,
                                  indexByteCount,
                                  commands.indexBuffer.base + entry->indexBufferOffset + runningIndexCount,
                                  GL_STATIC_DRAW );

                    //glDrawElementsBaseVertex( GL_TRIANGLES, mesh_data->indexCount, GL_UNSIGNED_INT, (void *)0, mesh_data->indexStartOffset );
                    glDrawElements( GL_TRIANGLES, mesh_data->indexCount, GL_UNSIGNED_INT, (void *)0 );

                    runningVertexCount += mesh_data->vertexCount;
                    runningIndexCount += mesh_data->indexCount;
                    mesh_data++;
                }

                // simClusterIndex
                glUniform1ui( gl->activeProgram->uniforms[2].locationId, 0 );

                totalDrawCalls += entry->meshCount;
                totalVertexCount += runningVertexCount;
                totalPrimitiveCount += runningIndexCount / 3;
                totalMeshCount += entry->meshCount;
            } break;

            default:
            {
                LOG( "ERROR :: Unsupported RenderEntry type [%d]", entryHeader->type );
                INVALID_CODE_PATH;
            } break;
        }

        baseAddress += entryHeader->size;
        ASSERT_GL_STATE;
    }

    // Don't keep active program for next frame
    OpenGLUseProgram( ShaderProgramName::None, commands, gl );

#if !RELEASE
    glEndQuery( GL_PRIMITIVES_GENERATED );

    DebugState* debugState = (DebugState*)gameMemory->debugStorage;
    debugState->totalDrawCalls = totalDrawCalls;
    debugState->totalVertexCount = totalVertexCount;
    debugState->totalPrimitiveCount = totalPrimitiveCount;
    debugState->totalInstanceCount = totalInstanceCount;
    debugState->totalMeshCount = totalMeshCount;

    // Ask for query object result asynchronously, otherwise we would stall the GPU until results are available
    u32 queryResult = 0;
    glGetQueryObjectuiv( gl->queryObjectId, GL_QUERY_RESULT_AVAILABLE, &queryResult );
    if( queryResult == GL_TRUE )
    {
        glGetQueryObjectuiv( gl->queryObjectId, GL_QUERY_RESULT, &queryResult );
        debugState->totalGeneratedVerticesCount = queryResult;
    }

    ASSERT_GL_STATE;
#endif
}
