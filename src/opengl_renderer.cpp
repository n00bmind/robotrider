
GL_DEBUG_CALLBACK(OpenGLDebugCallback)
{
    const GLchar *errorMsg = message;
    ASSERT( !"OpenGL error" );
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

    if( glDebugMessageCallbackARB )
    {
        glEnable( GL_DEBUG_OUTPUT_SYNCHRONOUS );
        glDebugMessageCallbackARB( OpenGLDebugCallback, 0 );
    }

    ASSERT_GL_STATE;
    return info;
}

internal m4
CreateProjection( r32 aspectRatio )
{
    r32 n = 0.1f;
    r32 f = 100.0f;
    r32 d = f - n;
    r32 a = aspectRatio;
    // TODO 
    r32 fovy = 45;

    m4 result =
    {{
        { 2*n/a,      0,           0,            0 },
        {     0,    2*n,           0,            0 },
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
    local_persistent CubeThing cubes[1];

    glViewport( 0, 0, commands->width, commands->height );
    glClearColor( 0.95f, 0.95f, 0.95f, 1.0f );
    glClear( GL_COLOR_BUFFER_BIT );

    m4 projM = CreateProjection( (r32)commands->width / commands->height );

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
                { -0.5f,  -0.5f,  -1.0f },
                {  0.5f,  -0.5f,  -1.0f },
                {  0.0f,   0.5f,  -1.0f },
            },
            0
        };

        {
            glGenVertexArrays( 1, &dude.vao );
            GLuint vertexBuffer;
            glGenBuffers( 1, &vertexBuffer );

            glBindVertexArray( dude.vao );
            glBindBuffer( GL_ARRAY_BUFFER, vertexBuffer );
            glBufferData( GL_ARRAY_BUFFER, sizeof(dude.vertices), dude.vertices, GL_STATIC_DRAW );

            glVertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0 );
            glEnableVertexAttribArray( 0 );
        }

        for( int i = 0; i < ARRAYCOUNT(cubes); ++i )
        {
            CubeThing &cube = cubes[i];
            cube =
            {
                {
                    { -0.5f,    -0.5f,   -0.5f },
                    { -0.5f,    -0.5f,    0.5f },
                    {  0.5f,    -0.5f,   -0.5f },
                    {  0.5f,    -0.5f,    0.5f },
                },
                0
            };

            glGenVertexArrays( 1, &cube.vao );
            GLuint vertexBuffer;
            glGenBuffers( 1, &vertexBuffer );

            glBindVertexArray( cube.vao );
            glBindBuffer( GL_ARRAY_BUFFER, vertexBuffer );
            glBufferData( GL_ARRAY_BUFFER, sizeof(cube.vertices), cube.vertices, GL_STATIC_DRAW );

            glVertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0 );
            glEnableVertexAttribArray( 0 );
        }

        ASSERT_GL_STATE;
    }

    glUseProgram( shaderProgram );
    glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
    //glLineWidth( 3 );
    glBindVertexArray( dude.vao );
    glDrawArrays( GL_TRIANGLES, 0, 3 );
    for( int i = 0; i < ARRAYCOUNT(cubes); ++i )
    {
        glBindVertexArray( cubes[i].vao );
        glDrawArrays( GL_TRIANGLE_STRIP, 0, 4 );
    }

}
