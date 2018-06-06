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
#ifndef __MESHGEN_H__
#define __MESHGEN_H__ 


struct MarchingCacheBuffers
{
    r32* bottomLayerSamples;
    r32* topLayerSamples;

    u32* bottomLayerVertexIndices;
    u32* middleLayerVertexIndices;
    u32* topLayerVertexIndices;

    u32 layerCellCount;
};

struct MarchingMeshPool
{
    MarchingCacheBuffers cacheBuffers;

    BucketArray<TexturedVertex> scratchVertices;
    BucketArray<u32> scratchIndices;

    MemoryBlock memorySentinel;
};

struct Mesh
{
    TexturedVertex* vertices;
    u32 vertexCount;
    u32* indices;
    u32 indexCount;

    m4 mTransform;

    MarchingMeshPool* ownerPool;
};



struct InflatedVertex
{
    v3 p;
    u32 refStart;
    u32 refCount;     // How many tris share this vertex
    m4Symmetric q;
    bool border;
};

struct InflatedTriangle
{
    u32 v[3];
    r64 error[4];
    bool deleted;
    bool dirty;
    v3 n;
};

// Back-references from vertices to the triangles they belong to
struct InflatedVertexRef
{
    u32 tId;        // Triangle index
    u32 tVertex;    // Vertex index (in triangle, so 0,1,2)
};

struct InflatedMesh
{
    Array<InflatedVertex> vertices;
    Array<InflatedTriangle> triangles;
};


struct Metaball
{
    v3 pCenter;
    r32 radiusMeters;
};

enum class IsoSurfaceType
{
    Cuboid,
    //HexPrism,
    Cylinder,
};


// NOTE MUST be read only data, so that it can be used by multiple threads!
struct GeneratorData
{
    // ...
};

struct UniversalCoords;
#define GENERATOR_FUNC(name) \
    Mesh* name( const GeneratorData* generatorData, const UniversalCoords& p, MarchingMeshPool* meshPool ) 
typedef GENERATOR_FUNC(GeneratorFunc);

struct Generator
{
    GeneratorFunc* func;
    GeneratorData* data;
};


struct GeneratorPathData
{
    GeneratorData header;

    // Center point and area around it for cube marching
    v3 pCenter;
    r32 areaSideMeters;

    // Current basis (Y is forward, Z is up)
    m4 basis;
    // Surface algorithm used
    IsoSurfaceType isoType;

    r32 thicknessSq;
    r32 minDistanceToTurn;
    r32 maxDistanceToTurn;
    r32 minDistanceToFork;
    r32 maxDistanceToFork;

    r32 distanceToNextTurn;
    r32 distanceToNextFork;
    m4* nextBasis;
    // TODO Support multiple forks?
    GeneratorPathData* nextFork;
};

struct StoredEntity;
struct GeneratorHullNodeData
{
    GeneratorData header;

    r32 areaSideMeters;
    r32 resolutionMeters;
};

#endif /* __MESHGEN_H__ */
