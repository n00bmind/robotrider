
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

internal void
OpenGLRenderToOutput( GameRenderCommands *commands )
{
    // TODO Store this in a 'global' OpenGL config
    local_persistent GLuint shaderProgram;
    // TODO Store this next to the input vertex data
    local_persistent GLuint VAO;

    glViewport( 0, 0, commands->width, commands->height );
    glClearColor( 0.95f, 0.95f, 0.95f, 1.0f );
    glClear( GL_COLOR_BUFFER_BIT );

    if( !commands->initialized )
    {
        commands->initialized = true;

        // Compile shaders
        const char *vertexShaderSource =
#include "shaders/null_vs.glsl"
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
            return;
        }

        const char *fragmentShaderSource =
#include "shaders/black_fs.glsl"
            ;

        GLuint fragmentShader = glCreateShader( GL_FRAGMENT_SHADER );
        glShaderSource( fragmentShader, 1, &fragmentShaderSource, NULL );
        glCompileShader( fragmentShader );

        glGetShaderiv( fragmentShader, GL_COMPILE_STATUS, &success );
        if( !success )
        {
            glGetShaderInfoLog( fragmentShader, 512, NULL, infoLog );
            LOG( "ERROR :: Fragment shader compilation failed!\n%s\n", infoLog );
            return;
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
            return;
        }

        glDeleteShader( vertexShader );
        glDeleteShader( fragmentShader );

        // Bind Vertex Array Object with all the needed configuration
        float vertices[] =
        {
            -0.5f,  -0.5f,  0.0f,
            0.5f,  -0.5f,  0.0f,
            0.0f,   0.5f,  0.0f
        };

        glGenVertexArrays( 1, &VAO );
        GLuint vertexBuffer;
        glGenBuffers( 1, &vertexBuffer );

        glBindVertexArray( VAO );
        glBindBuffer( GL_ARRAY_BUFFER, vertexBuffer );
        glBufferData( GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW );

        glVertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0 );
        glEnableVertexAttribArray( 0 );
    }

    glUseProgram( shaderProgram );
    glBindVertexArray( VAO );
    glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
    glLineWidth( 3 );
    glDrawArrays( GL_TRIANGLES, 0, 3 );
}
