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


struct Player
{
    Mesh mesh;
};


// NOTE
//
///// HOW OUR UNIVERSE WILL WORK /////
//
// We will partition our universe in axis-aligned 'clusters'.
// At any given moment, one of this clusters will be the 'origin of the universe', meaning its center will be
// our universe's (0, 0, 0) coord, and all active entities' positions will be relative to this point.
// Everytime the player moves we will check whether he's moved to a new cluster, and if so, we'll mark the new
// cluster as the origin and offset all entities (including the player and the camera) as necessary.
// (Only 'live' entity positions will be modified, 'dormant' entity data always contains the (integer) coordinate
// of the cluster they're in).
//
// We will maintain an apron around the origin where all entities will be active (something like a 3x3x3 cube probably),
// so when switching origins a certain number of clusters will have to be filled (built) if they were never visited
// before, or retrieved from the stored entities set and re-activated if we're returning to them.
// Our RNG will be totally deterministic, so any given cluster can (must) be filled in a deterministic way.

struct UniversalCoords
{
    // Cluster coordinates wrap around with int32, so our space wraps around itself around every X,Y,Z cartesian coordinate
    // This is a... 3-thorus?? :: https://en.wikipedia.org/wiki/Three-torus_model_of_the_universe
    v3i clusterP;
    v3 relativeP;
};

// Minimal stored version of an entity
// (for entities that have it)
struct StoredEntity
{
    UniversalCoords coords;
    // Dimensions of the aligned bounding box
    v3 dim;

    // We're not gonna generate using connected paths anymore. Instead, each chunk must be
    // correctly and deterministically generated everytime just based on its coordinates.
    // TODO Are we sure we want to have to regenerate the mesh?
    MeshGenerator generator;

    // NOTE This will be a bit of a mishmash for now

};

enum class EntityState
{
    Invalid = 0,
    Loaded,
    Active,
};

struct LiveEntity
{
    // TODO We may want to turn this into a pointer
    StoredEntity stored;

    // NOTE Mesh translation is always relative to the current sim-region center
    Mesh *mesh;

    // Newly generated entities are initially positioned relative to their root cluster
    // by the thread that generates them. In order to avoid synchronizing world origin
    // changes and weird concurrency glitches, their final position is recalculated
    // by the main thread after they've been already built (they remain invisible until
    // that happens).
    volatile EntityState state;
};

const r32 ClusterSizeMeters = VoxelsPerClusterAxis * VoxelSizeMeters;
typedef Grid3D<u8> ClusterVoxelGrid;

struct Cluster
{
    bool populated;
    // TODO Determine what the bucket size should be so we have just one bucket most of the time
    BucketArray<StoredEntity> entityStorage;

    // @Remove Just for visualization
    Array<BinaryVolume> volumes;
    ClusterVoxelGrid voxelGrid;
};

inline u32 ClusterHash( const v3i& key, u32 tableSize );
inline u32 EntityHash( const u32& key, u32 tableSize );

// 'Thickness' of the sim region on each side of the origin cluster
// (in number of clusters)
const int SimRegionWidth = 0;

enum MeshGeneratorType
{
    GenRoom,

    GenCOUNT
};

struct World
{
    v3 pPlayer;
    r32 playerPitch;
    r32 playerYaw;

    Player *player;

#if 0
    Array<Mesh> hullMeshes;
    Array<MeshGeneratorPath> pathsBuffer;
#endif

    // 'REAL' stuff
    //
    // For now this will be the primary storage for (stored) entities
    HashTable<v3i, Cluster, ClusterHash> clusterTable;
    // Scratch buffer for all the entities in the simulation region
    // (we take the clusters we want to simulate, expand the entities stored there to their live version, and then store them back)
    // (clusters around the player are always kept live)
    // TODO Is the previous sentence true?
    // TODO Actually do that at least with the entity coords (always used the value calculated from the stored coords)
    // instead of the weird en-mass translation!
    // TODO Investigate what a good bucket size is
    BucketArray<LiveEntity> liveEntities;
    // Handles to stored entities to allow arbitrary entity cross-referencing even for entities that move
    // across clusters
    HashTable<u32, StoredEntity*, EntityHash> entityRefs;

    // Coordinates of the 'origin' cluster
    v3i originClusterP;
    v3i lastOriginClusterP;

    r32 marchingAreaSize;
    r32 marchingCubeSize;

    // TODO Should this be aligned and/or padded for cache niceness?
    MarchingCacheBuffers* cacheBuffers;
    MeshPool* meshPools;

    MeshGeneratorJob generatorJobs[PLATFORM_MAX_JOBQUEUE_JOBS];
    u32 lastAddedJob;
};

#endif /* __WORLD_H__ */
