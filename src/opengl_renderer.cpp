
GL_DEBUG_CALLBACK(OpenGLDebugCallback)
{
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

    if( severity != GL_DEBUG_SEVERITY_NOTIFICATION )
    {
        ASSERT( !"OpenGL error" );
    }
}

internal OpenGLInfo
OpenGLGetInfo( b32 modernContext )
{
    OpenGLInfo result = {};
    result.modernContext = modernContext;
    result.vendor = (const char *)glGetString( GL_VENDOR );
    result.renderer = (const char *)glGetString( GL_RENDERER );
    result.version = (const char *)glGetString( GL_VERSION );

    if( modernContext )
    {
        result.SLversion = (const char *)glGetString( GL_SHADING_LANGUAGE_VERSION );

        GLint n, i;
        glGetIntegerv(GL_NUM_EXTENSIONS, &n);
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

internal m4
CreatePerspectiveMatrix( r32 aspectRatio, r32 fovYDeg )
{
    r32 n = 0.1f;
    r32 f = 100.0f;
    r32 d = f - n;
    r32 a = aspectRatio;
    r32 fovy = Radians( fovYDeg );
    r32 ctf = 1 / (r32)tan( fovy / 2 );

    m4 result =
    {{
        { ctf/a,      0,           0,            0 },
        {     0,    ctf,           0,            0 },
        {     0,      0,    -(f+n)/d,     -2*f*n/d },
        {     0,      0,          -1,            0 } 
    }};

    return result;
}

internal m4
CreateOrthographicMatrix( r32 width, r32 height )
{
    r32 w = width;
    r32 h = -height;

    m4 result =
    {{
        { 2.0f/w,        0,      0,     0 },
        {      0,   2.0f/h,      0,     0 },
        {      0,        0,     -1,     0 },
        {     -1,        1,      0,     1 },
    }};

    return result;
}

internal OpenGLInfo
OpenGLInit( OpenGLState &gl, bool modernContext )
{
    OpenGLInfo info = OpenGLGetInfo( modernContext );

#if DEBUG
    if( glDebugMessageCallbackARB )
    {
        glEnable( GL_DEBUG_OUTPUT_SYNCHRONOUS );
        glDebugMessageCallbackARB( OpenGLDebugCallback, 0 );
    }
#endif

#if 1
    // According to Muratori, changing VAOs is inefficient and not really used anymore (?)
    // (gotta ask him about that claim..)
    GLuint dummyVAO;
    glGenVertexArrays( 1, &dummyVAO );
    glBindVertexArray( dummyVAO );
#endif

    glGenBuffers( 1, &gl.vertexBuffer );
    glBindBuffer( GL_ARRAY_BUFFER, gl.vertexBuffer );
    glGenBuffers( 1, &gl.indexBuffer );
    glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, gl.indexBuffer );

    // Compile shaders
    const char *vertexShaderSource =
#include "shaders/default.vs.glsl"
        ;

    GLuint vertexShader = glCreateShader( GL_VERTEX_SHADER );
    glShaderSource( vertexShader, 1, &vertexShaderSource, NULL );
    glCompileShader( vertexShader );

    int  success;
    char infoLog[512];
    glGetShaderiv( vertexShader, GL_COMPILE_STATUS, &success );
    if( !success )
    {
        glGetShaderInfoLog( vertexShader, 512, NULL, infoLog );
        LOG( "ERROR :: Vertex shader compilation failed!\n%s\n", infoLog );
        INVALID_CODE_PATH
    }

    const char *fragmentShaderSource =
#include "shaders/black.fs.glsl"
        ;

    GLuint fragmentShader = glCreateShader( GL_FRAGMENT_SHADER );
    glShaderSource( fragmentShader, 1, &fragmentShaderSource, NULL );
    glCompileShader( fragmentShader );

    glGetShaderiv( fragmentShader, GL_COMPILE_STATUS, &success );
    if( !success )
    {
        glGetShaderInfoLog( fragmentShader, 512, NULL, infoLog );
        LOG( "ERROR :: Fragment shader compilation failed!\n%s\n", infoLog );
        INVALID_CODE_PATH
    }

    gl.shaderProgram = glCreateProgram();
    glAttachShader( gl.shaderProgram, vertexShader );
    glAttachShader( gl.shaderProgram, fragmentShader );
    glLinkProgram( gl.shaderProgram );

    glGetProgramiv( gl.shaderProgram, GL_LINK_STATUS, &success );
    if( !success )
    {
        glGetProgramInfoLog( gl.shaderProgram, 512, NULL, infoLog );
        LOG( "ERROR :: Shader program linkage failed!\n%s\n", infoLog );
        INVALID_CODE_PATH
    }

    glDeleteShader( vertexShader );
    glDeleteShader( fragmentShader );

    gl.transformUniformId = glGetUniformLocation( gl.shaderProgram, "mTransform" );
    gl.pAttribId = glGetAttribLocation( gl.shaderProgram, "pIn" );
    gl.uvAttribId = glGetAttribLocation( gl.shaderProgram, "uvIn" );
    gl.cAttribId = glGetAttribLocation( gl.shaderProgram, "cIn" );

    ASSERT_GL_STATE;
    return info;
}

internal void
OpenGLRenderImGui( ImDrawData *drawData )
{
    OpenGLState &gl = *((OpenGLState *)drawData->UserData);

    // Avoid rendering when minimized, scale coordinates for retina displays (screen coordinates != framebuffer coordinates)
    ImGuiIO& io = ImGui::GetIO();
    int fbWidth = (int)(io.DisplaySize.x * io.DisplayFramebufferScale.x);
    int fbHeight = (int)(io.DisplaySize.y * io.DisplayFramebufferScale.y);
    if (fbWidth == 0 || fbHeight == 0)
        return;
    drawData->ScaleClipRects( io.DisplayFramebufferScale );
/*
    // Backup GL state (probably not too necessary)
    GLenum lastActiveTexture; glGetIntegerv(GL_ACTIVE_TEXTURE, (GLint*)&lastActiveTexture);
    GLint lastProgram; glGetIntegerv(GL_CURRENT_PROGRAM, &lastProgram);
    GLint lastTexture; glGetIntegerv(GL_TEXTURE_BINDING_2D, &lastTexture);
    GLint lastViewport[4]; glGetIntegerv(GL_VIEWPORT, lastViewport);
    GLint lastScissorBox[4]; glGetIntegerv(GL_SCISSOR_BOX, lastScissorBox);
    GLenum lastBlendSrcRGB; glGetIntegerv(GL_BLEND_SRC_RGB, (GLint*)&lastBlendSrcRGB);
    GLenum lastBlendDstRGB; glGetIntegerv(GL_BLEND_DST_RGB, (GLint*)&lastBlendDstRGB);
    GLenum lastBlendSrcAlpha; glGetIntegerv(GL_BLEND_SRC_ALPHA, (GLint*)&lastBlendSrcAlpha);
    GLenum lastBlendDstAlpha; glGetIntegerv(GL_BLEND_DST_ALPHA, (GLint*)&lastBlendDstAlpha);
    GLenum lastBlendEqRGB; glGetIntegerv(GL_BLEND_EQUATION_RGB, (GLint*)&lastBlendEqRGB);
    GLenum lastBlendEqAlpha; glGetIntegerv(GL_BLEND_EQUATION_ALPHA, (GLint*)&lastBlendEqAlpha);
    GLboolean lastEnableBlend = glIsEnabled(GL_BLEND);
    GLboolean lastEnableCullFace = glIsEnabled(GL_CULL_FACE);
    GLboolean lastEnableDepthTest = glIsEnabled(GL_DEPTH_TEST);
    GLboolean lastEnableScissorTest = glIsEnabled(GL_SCISSOR_TEST);
*/
    // Setup render state: alpha-blending enabled, no face culling, no depth testing, scissor enabled
    glActiveTexture( GL_TEXTURE0 );
    glEnable( GL_BLEND );
    glBlendEquation( GL_FUNC_ADD );
    glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
    glDisable( GL_CULL_FACE );
    glDisable( GL_DEPTH_TEST );
    glEnable( GL_SCISSOR_TEST );

    // Setup viewport, orthographic projection matrix
    glViewport( 0, 0, (GLsizei)fbWidth, (GLsizei)fbHeight );
    m4 orthoProj = CreateOrthographicMatrix( io.DisplaySize.x, io.DisplaySize.y );
    glUseProgram( gl.imGui.program );
    glUniform1i( gl.imGui.texUniformId, 0 );
    glUniformMatrix4fv( gl.imGui.projUniformId, 1, GL_FALSE, orthoProj.e[0] );

    for( int cmdListIdx = 0; cmdListIdx < drawData->CmdListsCount; cmdListIdx++ )
    {
        const ImDrawList* cmdList = drawData->CmdLists[cmdListIdx];
        const ImDrawIdx* indexBufferOffset = 0;

        glEnableVertexAttribArray( gl.imGui.pAttribId );
        glEnableVertexAttribArray( gl.imGui.uvAttribId );
        glEnableVertexAttribArray( gl.imGui.cAttribId );

        glVertexAttribPointer( gl.imGui.pAttribId, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (GLvoid*)OFFSETOF(ImDrawVert, pos) );
        glVertexAttribPointer( gl.imGui.uvAttribId, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (GLvoid*)OFFSETOF(ImDrawVert, uv) );
        glVertexAttribPointer( gl.imGui.cAttribId, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(ImDrawVert), (GLvoid*)OFFSETOF(ImDrawVert, col) );
        glBufferData( GL_ARRAY_BUFFER, (GLsizeiptr)cmdList->VtxBuffer.Size * sizeof(ImDrawVert), (const GLvoid*)cmdList->VtxBuffer.Data, GL_STREAM_DRAW );
        glBufferData( GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)cmdList->IdxBuffer.Size * sizeof(ImDrawIdx), (const GLvoid*)cmdList->IdxBuffer.Data, GL_STREAM_DRAW );

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
/*
    // Restore modified GL state
    glUseProgram(lastProgram);
    glBindTexture(GL_TEXTURE_2D, lastTexture);
    glActiveTexture(lastActiveTexture);
    glBindVertexArray(last_vertex_array);
    glBindBuffer(GL_ARRAY_BUFFER, lastArrayBuffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, last_element_array_buffer);
    glBlendEquationSeparate(lastBlendEqRGB, lastBlendEqAlpha);
    glBlendFuncSeparate(lastBlendSrcRGB, lastBlendDstRGB, lastBlendSrcAlpha, lastBlendDstAlpha);
    if (lastEnableBlend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    if (lastEnableCullFace) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    if (lastEnableDepthTest) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (lastEnableScissorTest) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
    glViewport(lastViewport[0], lastViewport[1], (GLsizei)lastViewport[2], (GLsizei)lastViewport[3]);
    glScissor(lastScissorBox[0], lastScissorBox[1], (GLsizei)lastScissorBox[2], (GLsizei)lastScissorBox[3]);    
*/
}

internal void
OpenGLInitImGui( OpenGLState &gl )
{
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
    gl.imGui.pAttribId	    = glGetAttribLocation( gl.imGui.program, "Position" );
    gl.imGui.uvAttribId	    = glGetAttribLocation( gl.imGui.program, "UV" );
    gl.imGui.cAttribId	    = glGetAttribLocation( gl.imGui.program, "Color" );

    //glGenBuffers(1, &g_VboHandle);
    //glGenBuffers(1, &g_ElementsHandle);

/*
    // FIXME These will have to be specified everytime we render the display lists
    glEnableVertexAttribArray(gl.imGui.pAttribId);
    glEnableVertexAttribArray(g_AttribLocationUV);
    glEnableVertexAttribArray(g_AttribLocationColor);

    glVertexAttribPointer(gl.imGui.pAttribId, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (GLvoid*)OFFSETOF(ImDrawVert, pos));
    glVertexAttribPointer(g_AttribLocationUV, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (GLvoid*)OFFSETOF(ImDrawVert, uv));
    glVertexAttribPointer(g_AttribLocationColor, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(ImDrawVert), (GLvoid*)OFFSETOF(ImDrawVert, col));
*/

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

    // Store our identifier
    io.Fonts->TexID = (void *)(intptr_t)gl.imGui.fontTexture;

    // TODO Override memory allocation functions!
    io.RenderDrawListsFn = OpenGLRenderImGui;
    ImGui::GetCurrentContext()->RenderDrawData.UserData = &gl;
}

internal void
OpenGLRenderToOutput( OpenGLState &gl, GameRenderCommands &commands )
{
    glViewport( 0, 0, commands.width, commands.height );

    m4 mProjView = CreatePerspectiveMatrix( (r32)commands.width / commands.height, 50 );
    mProjView = mProjView * commands.mCamera;

    glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
    //glLineWidth( 3 );

    RenderBuffer &buffer = commands.renderBuffer;
    for( u32 baseAddress = 0; baseAddress < buffer.size; /**/ )
    {
        RenderEntry *entryHeader = (RenderEntry *)(buffer.base + baseAddress);

        switch( entryHeader->type )
        {
            case RenderEntryType::RenderEntryClear:
            {
                RenderEntryClear *entry = (RenderEntryClear *)entryHeader;

                glClearColor( entry->color.r, entry->color.g, entry->color.b, entry->color.a ); 
                glClear( GL_COLOR_BUFFER_BIT );

                baseAddress += sizeof(*entry);
            } break;

            case RenderEntryType::RenderEntryGroup:
            {
                RenderEntryGroup *entry = (RenderEntryGroup *)entryHeader;

                glUseProgram( gl.shaderProgram );

                // Bind Vertex Array Object with all the needed configuration
                u32 vertexBuffer;
                u32 elementBuffer;

                glGenBuffers( 1, &vertexBuffer );
                glBindBuffer( GL_ARRAY_BUFFER, vertexBuffer );
                glBufferData( GL_ARRAY_BUFFER, entry->vertexCount*sizeof(v3), entry->vertices, GL_STATIC_DRAW );
                glEnableVertexAttribArray( 0 );
                glVertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0 );

                glGenBuffers( 1, &elementBuffer );
                glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, elementBuffer );
                glBufferData( GL_ELEMENT_ARRAY_BUFFER, entry->indexCount*sizeof(u32), entry->indices, GL_STATIC_DRAW );

                m4 mTransform = mProjView * (*entry->mTransform);
                GLint transformId = glGetUniformLocation( gl.shaderProgram, "mTransform" );
                glUniformMatrix4fv( transformId, 1, GL_TRUE, mTransform.e[0] );

                glDrawElements( GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0 );

                baseAddress += sizeof(*entry);
            } break;

            case RenderEntryType::RenderEntryTexturedTris:
            {
                RenderEntryTexturedTris *entry = (RenderEntryTexturedTris *)entryHeader;

                glUseProgram( gl.shaderProgram );
                GLint transformId = gl.transformUniformId;
                glUniformMatrix4fv( transformId, 1, GL_TRUE, mProjView.e[0] );

                // Material *matPtr = entry->materialArray;

                GLuint pAttribId = gl.pAttribId;
                GLuint uvAttribId = gl.uvAttribId;
                GLuint cAttribId = gl.cAttribId;

                // TODO This call should be done just once per frame at the very beginning..
                glBufferData( GL_ARRAY_BUFFER,
                              commands.vertexBuffer.count * sizeof(TexturedVertex),
                              commands.vertexBuffer.base,
                              GL_STREAM_DRAW );

                glEnableVertexAttribArray( pAttribId );
                //glEnableVertexAttribArray( uvAttribId );
                //glEnableVertexAttribArray( cAttribId );

                glVertexAttribPointer( pAttribId, 3, GL_FLOAT, false, sizeof(TexturedVertex), (void *)OFFSETOF(TexturedVertex, p) );
                //glVertexAttribPointer( uvAttribId, 2, GL_FLOAT, false, sizeof(TexturedVertex), (void *)OFFSETOF(TexturedVertex, uv) );
                //glVertexAttribPointer( cAttribId, 4, GL_UNSIGNED_BYTE, true, sizeof(TexturedVertex), (void *)OFFSETOF(TexturedVertex, color)  );

                // TODO This call should be done just once per frame at the very beginning..
                glBufferData( GL_ELEMENT_ARRAY_BUFFER,
                              commands.indexBuffer.count * sizeof(u32),
                              commands.indexBuffer.base,
                              GL_STREAM_DRAW );

                glDrawElements( GL_TRIANGLES, 3 * entry->triCount, GL_UNSIGNED_INT, (void *)(u64)entry->indexBufferOffset );

                glDisableVertexAttribArray( pAttribId );
                //glDisableVertexAttribArray( uvAttribId );
                //glDisableVertexAttribArray( cAttribId );

                glUseProgram( 0 );

                baseAddress += sizeof(*entry);
            } break;

            INVALID_DEFAULT_CASE
        }

        ASSERT_GL_STATE;
    }
}
