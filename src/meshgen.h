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

#if NON_UNITY_BUILD
#include "data_types.h"
#include "renderer.h"
#include "debugstats.h"
#endif


// 2D slice of a 3D sampled area, allowing reuse of sampled values and generated vertices
struct IsoSurfaceSamplingCache
{
    f32* bottomLayerSamples;
    f32* topLayerSamples;

    i32* bottomLayerVertexIndices;
    i32* middleLayerVertexIndices;
    i32* topLayerVertexIndices;

    v2i cellsPerAxis;
};

struct MeshPool
{
    BucketArray<TexturedVertex> scratchVertices;
    BucketArray<i32> scratchIndices;

    MemoryBlock memorySentinel;
    i32 meshCount; 
};



struct FQSVertex
{
    TexturedVertex const* vertex;
    m4Symmetric q;
    v3 p;
    i32 refStart;
    i32 refCount;     // How many tris share this vertex
    bool border;
};

struct FQSTriangle
{
    f64 error[4];
    i32 v[3];
    v3 n;
    bool deleted;
    bool dirty;
};

// Back-references from vertices to the triangles they belong to
struct FQSVertexRef
{
    i32 tId;        // Triangle index
    i32 tVertex;    // Vertex index (in triangle, so 0,1,2)
};

struct FQSMesh
{
    Array<FQSVertex> vertices;
    Array<FQSTriangle> triangles;
};





struct MeshGeneratorData;
struct WorldCoords;
#define MESH_GENERATOR_FUNC(name) Mesh* name( const MeshGeneratorData& generatorData, const WorldCoords& entityCoords, \
                                              IsoSurfaceSamplingCache* samplingCache, BucketArray<TexturedVertex>* vertices, \
                                              BucketArray<i32>* indices ) 
typedef MESH_GENERATOR_FUNC(MeshGeneratorFunc);

struct StoredEntity;
struct LiveEntity;

struct MeshGeneratorJob
{
    const StoredEntity*     storedEntity;
    const v3i*              worldOriginClusterP;
    IsoSurfaceSamplingCache*   samplingCache;
    MeshPool*               meshPools;
    LiveEntity*             outputEntity;

    volatile bool occupied;
};


struct MeshGeneratorRoomData
{
    v3 dim;
};

// @Size Using this in the entity storage means every entity spans the maximum size which is very inefficient
struct MeshGeneratorData
{
    f32 areaSideMeters;
    f32 resolutionMeters;

    union
    {
        MeshGeneratorRoomData room;
        // ...
    };
};

struct MeshGenerator
{
    MeshGeneratorFunc* func;
    MeshGeneratorData data;
};

#if 0
enum class IsoSurfaceType
{
    Cuboid,
    //HexPrism,
    Cylinder,
};

struct MeshGeneratorPathData
{
    // Center point and area around it for cube marching
    v3 pCenter;
    f32 areaSideMeters;

    // Current basis (Y is forward, Z is up)
    m4 basis;
    // Surface algorithm used
    IsoSurfaceType isoType;

    f32 thicknessSq;
    f32 minDistanceToTurn;
    f32 maxDistanceToTurn;
    f32 minDistanceToFork;
    f32 maxDistanceToFork;

    f32 distanceToNextTurn;
    f32 distanceToNextFork;
    m4* nextBasis;
    // TODO Support multiple forks?
    MeshGeneratorPathData* nextFork;
};
#endif


enum SamplingDataType
{
    ClusterData,
    SimpleSurface,
};

struct SamplingData
{
    SamplingDataType type;
    // TODO Turn this into flags
    bool zeroThickness; 
};

struct SimpleSurfaceData
{
    SamplingData header;

    m4 invWorldTransform;
    i32 surfaceType;
};

SimpleSurfaceData InitSimpleSurfaceData()
{
    SamplingData header = { SamplingDataType::SimpleSurface, true };
    SimpleSurfaceData result = { header };
    return result;
}


#define SURFACE_LIST(x) \
    x(MechanicalPart)   \
    x(Torus)            \
    x(HollowCube)       \
    x(Devil)            \
    x(QuarticCylinder)  \
    x(TangleCube)       \
    x(Genus2)           \

    //x(TrefoilKnot)      \   // FIXME
    //x(Cone)            \
    //x(LinkedTorii)      \

STRUCT_ENUM(SimpleSurface, SURFACE_LIST);
#undef SURFACE_LIST


#define VALUES(x)     \
    x(DualContouring) \
    x(MarchingCubes)  \

STRUCT_ENUM(ContouringTechnique, VALUES)
#undef VALUES


enum class DCComputeMethod
{
    Average,
    QEFClassic,
    QEFProbabilistic,
    QEFProbabilisticDouble,
};

struct DCSettings
{
    DCComputeMethod cellPointsComputationMethod;
    f32 sigmaN;
    f32 sigmaNDouble;
    bool approximateEdgeIntersection;
    bool clampCellPoints;
};



#define ISO_SURFACE_FUNC(name) float name( WorldCoords const& worldP, SamplingData const* samplingData )
typedef ISO_SURFACE_FUNC(IsoSurfaceFunc);

ISO_SURFACE_FUNC(RoomSurfaceFunc);
ISO_SURFACE_FUNC( SimpleSurfaceFunc );


void InitMeshPool( MeshPool* pool, MemoryArena* arena, sz size );
Mesh* AllocateMesh( MeshPool* pool, int vertexCount, int indexCount );
Mesh* AllocateMeshFromScratchBuffers( MeshPool* pool );
void ClearScratchBuffers( MeshPool* pool );
inline Mesh CreateMeshFromBuffers( BucketArray<TexturedVertex> const& vertices, BucketArray<i32> const& indices, MemoryArena* arena );
void ReleaseMesh( Mesh** mesh );

IsoSurfaceSamplingCache InitSurfaceSamplingCache( MemoryArena* arena, v2i const& cellsPerAxis );
void ClearVertexCaches( IsoSurfaceSamplingCache* samplingCache, bool clearBottomLayer );
void SwapTopAndBottomLayers( IsoSurfaceSamplingCache* samplingCache );

void MarchCube( const v3& cellCornerWorldP, const v2i& gridCellP, v2i const& cellsPerAxis, f32 cellSizeMeters,
                IsoSurfaceSamplingCache* samplingCache, BucketArray<TexturedVertex>* vertices, BucketArray<i32>* indices,
                const bool interpolate = true );
void MarchVolumeFast( WorldCoords const& worldP, v3 const& volumeSideMeters, f32 cellSizeMeters, IsoSurfaceFunc* sampleFunc,
                 SamplingData const* samplingData, IsoSurfaceSamplingCache* samplingCache, BucketArray<TexturedVertex>* vertices,
                 BucketArray<i32>* indices, const bool interpolate = true );

void DCVolume( WorldCoords const& worldP, v3 const& volumeSizeMeters, f32 cellSizeMeters, IsoSurfaceFunc* sampleFunc, SamplingData* samplingData,
          BucketArray<TexturedVertex>* vertices, BucketArray<i32>* indices, MemoryArena* arena, MemoryArena* tmpArena, DCSettings const& settings );

Mesh* ConvertToIsoSurfaceMesh( const Mesh& sourceMesh, f32 drawingDistance, int displayedLayer, IsoSurfaceSamplingCache* samplingCache,
                               MeshPool* meshPool, MemoryArena* tmpArena, RenderCommands* renderCommands );



#endif /* __MESHGEN_H__ */
