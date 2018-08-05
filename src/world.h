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
// We will partition our universe in axis-aligned 'clusters', their length a (probably big) multiple of
// what we end up using for the hull chunks (so N chunks in every XYZ direction will form a cluster).
// At any given moment, one of this clusters will be the 'origin of the universe', meaning its center will be
// our universe's (0, 0, 0) coord, and all active entities' positions will be relative to this point.
// Everytime the player moves we will check whether he's moved to a new cluster, and if so, we'll mark the new
// cluster as the origin and offset all entities (including the player and the camera) as necessary.
// (Only 'hi' entity positions will be modified, 'lo' entity data always contains the absolute cluster they're in).
//
// We will maintain an apron around the origin where all entities will be active (something like a 3x3x3 cube probably),
// so when switching origins a certain number of clusters will have to be filled (built) if they were never visited
// before, or retrieved from the stored entities set and re-activated if we're returning to them.
// Our RNG will be totally deterministic, so any given cluster can (must) be filled in a deterministic way.
// (this implies getting rid of the always-connected pipes, at least in the way we do them now).

struct UniversalCoords
{
    // Cluster coordinates wrap around with int32, so our space wraps around itself around every X,Y,Z cartesian coordinate
    // This is a... 3-thorus?? :: https://en.wikipedia.org/wiki/Three-torus_model_of_the_universe
    v3i pCluster;
    v3 pClusterOffset;
};

// Minimal stored version of an entity
// (for entities that have it)
struct StoredEntity
{
    UniversalCoords pUniverse;

    // We're not gonna generate using connected paths anymore. Instead, each chunk must be
    // correctly and deterministically generated everytime just based on its coordinates.
    // TODO Are we sure we want to have to regenerate the mesh?
    Generator* generator;
};

enum class EntityState
{
    Invalid = 0,
    Loaded,
    Active,
};

struct LiveEntity
{
    StoredEntity stored;

    // NOTE Mesh translation is always relative to the current sim-region center
    Mesh *mesh;

    // Newly generated entities are initially positioned relative to their root cluster
    // by the thread that generates them. In order to avoid synchronizing world origin
    // changes and weird concurrency glitches, their final position is recalculated
    // by the main thread after they've been already built (they remain invisible until
    // that happens).
    EntityState state;
};

#define CLUSTER_HALF_SIZE_METERS 200

struct Cluster
{
    bool populated;
    // TODO Determine what the bucket size should be so we have just one bucket most of the time
    BucketArray<StoredEntity> entityStorage;
};

struct GeneratorJob
{
    const StoredEntity* storedEntity;
    const v3i*          pWorldOrigin;
    MarchingMeshPool*   meshPools;
    LiveEntity*         outputEntity;

    volatile bool occupied;
};

// 'Thickness' in clusters of the sim region on each side of the origin cluster
#define SIM_REGION_WIDTH 1

struct World
{
    // TODO Move to the player struct
    v3 pPlayer;
    r32 playerPitch;
    r32 playerYaw;

    Player *player;

#if 0
    Array<Mesh> hullMeshes;
    Array<GeneratorPath> pathsBuffer;
#endif

    // 'REAL' stuff
    //
    // For now this will be the primary storage for (stored) entities
    HashTable<v3i, Cluster> clusterTable;
    // Scratch buffer for all the entities in the simulation region
    // (we take the clusters we want to simulate, expand the entities stored there to their live version, and then store them back)
    // (clusters around the player are always kept live)
    // TODO Is the previous sentence true?
    // TODO Investigate what a good bucket size is
    BucketArray<LiveEntity> liveEntities;
    // Handles to stored entities to allow arbitrary entity cross-referencing even for entities that move
    // across clusters
    HashTable<u32, StoredEntity*> entityRefs;

    // Coordinates of the current cluster
    v3i pWorldOrigin;
    v3i pLastWorldOrigin;

    r32 marchingAreaSize;
    r32 marchingCubeSize;

    Generator meshGenerators[16];
    // TODO Create one of these per available worker thread
    // (give threads an index they can use in the resulting array)
    // TODO Should this be aligned and/or padded for cache niceness?
    MarchingMeshPool* meshPools;

    GeneratorJob generatorJobs[PLATFORM_MAX_JOBQUEUE_JOBS];
    u32 lastAddedJob;
};

#endif /* __WORLD_H__ */
