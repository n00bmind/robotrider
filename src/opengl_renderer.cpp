
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
OpenGLInit( bool modernContext )
{
    OpenGLInfo info = OpenGLGetInfo( modernContext );

#if DEBUG
    if( glDebugMessageCallbackARB )
    {
        glEnable( GL_DEBUG_OUTPUT_SYNCHRONOUS );
        glDebugMessageCallbackARB( OpenGLDebugCallback, 0 );
    }
#endif

    // TODO According to Muratori, changing VAOs is inefficient and not really used anymore
    // but they sti _must_ be used when in the Core profile, so..
    GLuint dummyVAO;
    glGenVertexArrays( 1, &dummyVAO );
    glBindVertexArray( dummyVAO );

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
OpenGLRenderToOutput( GameRenderCommands *commands )
{
    // TODO Store this in a 'global' OpenGL config
    local_persistent GLuint shaderProgram;
    local_persistent FlyingDude dude;
    local_persistent CubeThing cubes[256];

    glViewport( 0, 0, commands->width, commands->height );
    glClearColor( 0.95f, 0.95f, 0.95f, 1.0f );
    glClear( GL_COLOR_BUFFER_BIT );

    m4 projM = OpenGLCreatePerspectiveMatrix( (r32)commands->width / commands->height, 120 );

    if( !commands->initialized )
    {
        commands->initialized = true;

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

        shaderProgram = glCreateProgram();
        glAttachShader( shaderProgram, vertexShader );
        glAttachShader( shaderProgram, fragmentShader );
        glLinkProgram( shaderProgram );

        glGetProgramiv( shaderProgram, GL_LINK_STATUS, &success );
        if( !success )
        {
            glGetProgramInfoLog( shaderProgram, 512, NULL, infoLog );
            LOG( "ERROR :: Shader program linkage failed!\n%s\n", infoLog );
            INVALID_CODE_PATH
        }

        glDeleteShader( vertexShader );
        glDeleteShader( fragmentShader );
        glUseProgram( shaderProgram );
        GLint projId = glGetUniformLocation( shaderProgram, "projM" );
        glUniformMatrix4fv( projId, 1, GL_TRUE, projM.e[0] );

        // Bind Vertex Array Object with all the needed configuration
        dude =
        {
            {
                { -0.5f,  -0.5f,  -10.0f },
                {  0.5f,  -0.5f,  -10.0f },
                {  0.0f,   0.5f,  -10.0f },
            },
            { 0, 1, 2 },
        };

        glGenBuffers( 1, &dude.vertexBuffer );
        glGenBuffers( 1, &dude.ebo );
        glBindBuffer( GL_ARRAY_BUFFER, dude.vertexBuffer );
        glBufferData( GL_ARRAY_BUFFER, sizeof(dude.vertices), dude.vertices, GL_STATIC_DRAW );
        glEnableVertexAttribArray( 0 );
        glVertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0 );
        glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, dude.ebo );
        glBufferData( GL_ELEMENT_ARRAY_BUFFER, sizeof(dude.indices), dude.indices, GL_STATIC_DRAW );

        for( int i = 0; i < ARRAYCOUNT(cubes); ++i )
        {
            r32 transX = ((i % 16) - 8) * 2.0f;
            r32 transZ = -(i / 16) * 2.0f;

            CubeThing &cube = cubes[i];
            cube =
            {
                {
                    { -0.5f,    0.0f,   -0.5f },
                    { -0.5f,    0.0f,    0.5f },
                    {  0.5f,    0.0f,   -0.5f },
                    {  0.5f,    0.0f,    0.5f },
                },
                {
                    0, 1, 2,
                    2, 1, 3
                },
                0,
                0,
                { transX, -2.0f, transZ },
            };

            glGenBuffers( 1, &cube.vertexBuffer );
            glGenBuffers( 1, &cube.ebo );

            glBindBuffer( GL_ARRAY_BUFFER, cube.vertexBuffer );
            glBufferData( GL_ARRAY_BUFFER, sizeof(cube.vertices), cube.vertices, GL_STATIC_DRAW );
            glEnableVertexAttribArray( 0 );
            glVertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0 );
            glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, cube.ebo );
            glBufferData( GL_ELEMENT_ARRAY_BUFFER, sizeof(cube.indices), cube.indices, GL_STATIC_DRAW );
        }

        ASSERT_GL_STATE;
    }

    glUseProgram( shaderProgram );
    glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
    //glLineWidth( 3 );

    m4 modelM = M4Identity();
    GLint modelId = glGetUniformLocation( shaderProgram, "modelM" );
    glUniformMatrix4fv( modelId, 1, GL_TRUE, modelM.e[0] );

    glBindBuffer( GL_ARRAY_BUFFER, dude.vertexBuffer );
    glVertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0 );
    glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, dude.ebo );
    glDrawElements( GL_TRIANGLES, 3, GL_UNSIGNED_INT, 0 );

    for( int i = 0; i < ARRAYCOUNT(cubes); ++i )
    {
        modelM = M4Translation( cubes[i].P );
        modelId = glGetUniformLocation( shaderProgram, "modelM" );
        glUniformMatrix4fv( modelId, 1, GL_TRUE, modelM.e[0] );

        glBindBuffer( GL_ARRAY_BUFFER, cubes[i].vertexBuffer );
        glVertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0 );
        glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, cubes[i].ebo );
        glDrawElements( GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0 );
    }
}
