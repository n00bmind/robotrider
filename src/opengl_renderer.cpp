
internal void
OpenGLRenderToOutput( GameRenderCommands *commands )
{
    glViewport( 0, 0, commands->width, commands->height );
    glClearColor( 1.0f, 0.0f, 1.0f, 0.0f );
    glClear( GL_COLOR_BUFFER_BIT );
}
