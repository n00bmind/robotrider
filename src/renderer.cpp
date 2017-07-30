
internal RenderGroup
CreateRenderGroup( FlyingDude &dude )
{
    RenderGroup entry = {};
    entry.vertices = dude.vertices;
    entry.vertexCount = ARRAYCOUNT( dude.vertices );
    entry.indices = dude.indices;
    entry.indexCount = ARRAYCOUNT( dude.indices );
    entry.mTransform = &dude.mTransform;

    return entry;
}

internal RenderGroup
CreateRenderGroup( CubeThing &cube )
{
    RenderGroup entry = {};
    entry.vertices = cube.vertices;
    entry.vertexCount = ARRAYCOUNT( cube.vertices );
    entry.indices = cube.indices;
    entry.indexCount = ARRAYCOUNT( cube.indices );
    entry.mTransform = &cube.mTransform;

    return entry;
}

internal void
PushRenderGroup( GameRenderCommands &commands, RenderGroup *entry )
{
    commands.renderEntries[commands.renderEntriesCount++] = entry;
}
