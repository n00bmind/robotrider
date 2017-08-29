
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

internal OpenGLInfo
OpenGLInit( OpenGLState &openGL, bool modernContext )
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

    glGenBuffers( 1, &openGL.vertexBuffer );
    glBindBuffer( GL_ARRAY_BUFFER, openGL.vertexBuffer );
    glGenBuffers( 1, &openGL.indexBuffer );
    glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, openGL.indexBuffer );

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

    openGL.shaderProgram = glCreateProgram();
    glAttachShader( openGL.shaderProgram, vertexShader );
    glAttachShader( openGL.shaderProgram, fragmentShader );
    glLinkProgram( openGL.shaderProgram );

    glGetProgramiv( openGL.shaderProgram, GL_LINK_STATUS, &success );
    if( !success )
    {
        glGetProgramInfoLog( openGL.shaderProgram, 512, NULL, infoLog );
        LOG( "ERROR :: Shader program linkage failed!\n%s\n", infoLog );
        INVALID_CODE_PATH
    }

    glDeleteShader( vertexShader );
    glDeleteShader( fragmentShader );

    openGL.transformUniformId = glGetUniformLocation( openGL.shaderProgram, "mTransform" );
    openGL.pAttribIndex = glGetAttribLocation( openGL.shaderProgram, "pIn" );
    openGL.uvAttribIndex = glGetAttribLocation( openGL.shaderProgram, "uvIn" );
    openGL.cAttribIndex = glGetAttribLocation( openGL.shaderProgram, "cIn" );

    ASSERT_GL_STATE;
    return info;
}

internal m4
OpenGLCreatePerspectiveMatrix( r32 aspectRatio, r32 fovYDeg )
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

internal void
OpenGLRenderToOutput( OpenGLState &openGL, GameRenderCommands &commands )
{
    glViewport( 0, 0, commands.width, commands.height );

    m4 mProjView = OpenGLCreatePerspectiveMatrix( (r32)commands.width / commands.height, 50 );
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

                glUseProgram( openGL.shaderProgram );

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
                GLint transformId = glGetUniformLocation( openGL.shaderProgram, "mTransform" );
                glUniformMatrix4fv( transformId, 1, GL_TRUE, mTransform.e[0] );

                glDrawElements( GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0 );

                baseAddress += sizeof(*entry);
            } break;

            case RenderEntryType::RenderEntryTexturedTris:
            {
                RenderEntryTexturedTris *entry = (RenderEntryTexturedTris *)entryHeader;

                glUseProgram( openGL.shaderProgram );
                GLint transformId = openGL.transformUniformId;
                glUniformMatrix4fv( transformId, 1, GL_TRUE, mProjView.e[0] );

                // Material *matPtr = entry->materialArray;

                GLuint pAttribIndex = openGL.pAttribIndex;
                GLuint uvAttribIndex = openGL.uvAttribIndex;
                GLuint cAttribIndex = openGL.cAttribIndex;

                // TODO This call should be done just once per frame at the very beginning..
                glBufferData( GL_ARRAY_BUFFER,
                              commands.vertexBuffer.count * sizeof(TexturedVertex),
                              commands.vertexBuffer.base,
                              GL_STREAM_DRAW );

                glEnableVertexAttribArray( pAttribIndex );
                //glEnableVertexAttribArray( uvAttribIndex );
                //glEnableVertexAttribArray( cAttribIndex );

                glVertexAttribPointer( pAttribIndex, 3, GL_FLOAT, false, sizeof(TexturedVertex), (void *)OFFSETOF(TexturedVertex, p) );
                //glVertexAttribPointer( uvAttribIndex, 2, GL_FLOAT, false, sizeof(TexturedVertex), (void *)OFFSETOF(TexturedVertex, uv) );
                //glVertexAttribPointer( cAttribIndex, 4, GL_UNSIGNED_BYTE, true, sizeof(TexturedVertex), (void *)OFFSETOF(TexturedVertex, color)  );

                // TODO This call should be done just once per frame at the very beginning..
                glBufferData( GL_ELEMENT_ARRAY_BUFFER,
                              commands.indexBuffer.count * sizeof(u32),
                              commands.indexBuffer.base,
                              GL_STREAM_DRAW );

                glDrawElements( GL_TRIANGLES, 3 * entry->triCount, GL_UNSIGNED_INT, (void *)(u64)entry->indexBufferOffset );

                glDisableVertexAttribArray( pAttribIndex );
                //glDisableVertexAttribArray( uvAttribIndex );
                //glDisableVertexAttribArray( cAttribIndex );

                glUseProgram( 0 );

                baseAddress += sizeof(*entry);
            } break;

            INVALID_DEFAULT_CASE
        }

        ASSERT_GL_STATE;
    }
}
