
internal void
PushFlyingDude( GameRenderCommands *commands, FlyingDude *dude )
{
    RenderGroup &entry = commands->renderEntries[commands->renderEntriesCount++]; 
    entry.vertices = dude->vertices;
    entry.vertexCount = 4;
    entry.indices = dude->indices;
    entry.indexCount = 12;
    entry.P = dude->P;
}

internal void
PushCubeThing( GameRenderCommands *commands, CubeThing *cube )
{
    RenderGroup &entry = commands->renderEntries[commands->renderEntriesCount++]; 
    entry.vertices = cube->vertices;
    entry.vertexCount = 4;
    entry.indices = cube->indices;
    entry.indexCount = 6;
    entry.P = cube->P;
}
