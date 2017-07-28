
internal RenderGroup
CreateRenderGroup( FlyingDude &dude )
{
    RenderGroup entry;
    entry.vertices = dude.vertices;
    entry.vertexCount = 4;
    entry.indices = dude.indices;
    entry.indexCount = 12;
    entry.mTransform = &dude.mTransform;

    return entry;
}

internal RenderGroup
CreateRenderGroup( CubeThing &cube )
{
    RenderGroup entry;
    entry.vertices = cube.vertices;
    entry.vertexCount = 4;
    entry.indices = cube.indices;
    entry.indexCount = 6;
    entry.mTransform = &cube.mTransform;

    return entry;
}

internal void
PushRenderGroup( GameRenderCommands &commands, RenderGroup *entry )
{
    commands.renderEntries[commands.renderEntriesCount++] = entry;
}
