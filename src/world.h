/*
The MIT License

Copyright (c) 2017 Oscar Peñas Pariente <oscarpp80@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#ifndef __WORLD_H__
#define __WORLD_H__ 

struct FlyingDude
{
    v3 vertices[3];
    u32 indices[3];

    m4 mTransform;
};

struct CubeThing
{
    v3 vertices[4];
    u32 indices[6];

    m4 mTransform;
};

struct Mesh
{
    TexturedVertex* vertices;
    u32* indices;
    u32 vertexCount;
    u32 indexCount;

    m4 mTransform;
};

struct HullChunk
{
    Mesh mesh;
    // TODO How're we gonna keep track of the generators so we can recreate everything
    // whenever the chunks get evicted because they're too far?
    HullChunk* prev;
    GenPath* generator;
};

struct World
{
    v3 pPlayer;
    r32 playerPitch;
    r32 playerYaw;

    FlyingDude *playerDude;

    CubeThing *cubes;
    u32 cubeCount;

    // 'real' stuff
    Array<Mesh> hullMeshes;
    //Array<HullChunk> hullChunksBuffer;
    Array<GenPath> pathsBuffer;

    r32 marchingAreaSize;
    r32 marchingCubeSize;
};

#endif /* __WORLD_H__ */
