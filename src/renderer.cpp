
internal void
InitRenderGroup( GameRenderCommands &commands, FlyingDude &dude )
{
    RenderGroup &entry = dude.renderGroup;
    entry = {};
    entry.vertices = dude.vertices;
    entry.vertexCount = ARRAYCOUNT( dude.vertices );
    entry.indices = dude.indices;
    entry.indexCount = ARRAYCOUNT( dude.indices );
    entry.mTransform = &dude.mTransform;
}

internal void
InitRenderGroup( GameRenderCommands &commands, CubeThing &cube )
{
    RenderGroup &entry = cube.renderGroup;
    entry = {};
    entry.vertices = cube.vertices;
    entry.vertexCount = ARRAYCOUNT( cube.vertices );
    entry.indices = cube.indices;
    entry.indexCount = ARRAYCOUNT( cube.indices );
    entry.mTransform = &cube.mTransform;
}

internal void
_PushRenderGroup( GameRenderCommands &commands, RenderGroup *entry )
{
    ASSERT( commands.renderEntriesCount < ARRAYCOUNT(commands.renderEntries) );
    commands.renderEntries[commands.renderEntriesCount++] = entry;
}

internal void
PushRenderGroup( GameRenderCommands &commands, FlyingDude &dude, bool rebuildGeometry = false )
{
    RenderGroup &entry = dude.renderGroup;
    if( rebuildGeometry )
    {
        entry = {};
        entry.vertices = dude.vertices;
        entry.vertexCount = ARRAYCOUNT( dude.vertices );
        entry.indices = dude.indices;
        entry.indexCount = ARRAYCOUNT( dude.indices );
    }
    entry.mTransform = &dude.mTransform;

    _PushRenderGroup( commands, &entry );
}

internal void
PushRenderGroup( GameRenderCommands &commands, CubeThing &cube, bool rebuildGeometry = false )
{
    RenderGroup &entry = cube.renderGroup;
    if( rebuildGeometry )
    {
        entry = {};
        entry.vertices = cube.vertices;
        entry.vertexCount = ARRAYCOUNT( cube.vertices );
        entry.indices = cube.indices;
        entry.indexCount = ARRAYCOUNT( cube.indices );
    }
    entry.mTransform = &cube.mTransform;

    _PushRenderGroup( commands, &entry );
}
