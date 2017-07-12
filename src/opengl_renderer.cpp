
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
    glViewport( 0, 0, commands->width, commands->height );
    glClearColor( 1.0f, 0.0f, 1.0f, 0.0f );
    glClear( GL_COLOR_BUFFER_BIT );
}
