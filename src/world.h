/*The MIT License

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

#if NON_UNITY_BUILD
#include "renderer.h"
#include "meshgen.h"
#include "platform.h"
#endif


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
// our universe's (0, 0, 0) coord, and the postitions of all active entities will be relative to this point.
// Everytime the player moves we will check whether he's moved to a new cluster, and if so, we'll mark the new
// cluster as the origin and offset all entities (including the player and the camera) as necessary.
// (Only 'live' entity positions will be modified, 'dormant' entity data always contains the (integer) coordinate
// of the cluster they're in).
//
// We will maintain an apron around the origin where all entities will be active (something like a 3x3x3 cube probably),
// so when switching origins a certain number of clusters will have to be filled (built) if they were never visited
// before, or retrieved from the stored entities set and re-activated if we're returning to them.
// Our RNG will be totally deterministic, so any given cluster can (must) be filled in a deterministic way.

struct WorldCoords
{
    // Cluster coordinates wrap around with int32, so our space wraps around itself around every X,Y,Z cartesian coordinate
    // This is a... 3-thorus?? :: https://en.wikipedia.org/wiki/Three-torus_model_of_the_universe
    v3 relativeP;
    v3i clusterP;
};

// Minimal stored version of an entity
// (for entities that have it)
struct StoredEntity
{
    WorldCoords worldP;
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

// NOTE Everything related to maze generation and connectivity is measured in voxel units
// Also, all integer voxel coords are relative to the "lower left" corner of the grid (cluster)
const f32 VoxelSizeMeters = 2.f;
const i32 VoxelsPerClusterAxis = 256;

const f32 ClusterSizeMeters = VoxelsPerClusterAxis * VoxelSizeMeters;
static_assert( (f32)(u32)(VoxelsPerClusterAxis * VoxelSizeMeters) == ClusterSizeMeters, "FAIL" );
const v3 ClusterHalfSize = V3( ClusterSizeMeters * 0.5f );

const int SampledRoomShellThickness = 3;

typedef Grid3D<u8> ClusterVoxelGrid;


struct Box
{
    v3 centerP;
    v3 halfSize;
};

// TODO Most of this will be in the stored entity when we turn rooms into that
struct Room
{
    v3i voxelP;
    v3i sizeVoxels;
    // TODO Make sure everything in the pipeline uses these right up until we send the meshes for rendering
    WorldCoords worldCenterP;
    v3 halfSize;
};

struct Hall
{
    // TODO It seems more and more pointless to have to compute and maintain both voxel and real world coords for everything
    Box sectionBoxes[3];

    v3i startP;
    v3i endP;
    // Encoded as 2 bits per axis (X == 0, Y == 1, Z == 2), first axis is LSB
    u8 axisOrder;
};

// TODO Pack minimal 8 byte coords for rooms and halls inline into this struct so everything is in contiguous memory and fast
struct ClusterSamplingData
{
    Array<Room> const& rooms;
    Array<Hall> const& halls;
    // Room index when sampling rooms, hall index for halls
    i32 sampledVolumeIndex;
};

enum VolumeFlags : u32
{
    VF_None = 0,
    HasRoom = 0x1,
    HasHall = 0x2,
};

struct BinaryVolume
{
    BinaryVolume* leftChild;
    BinaryVolume* rightChild;

    // Used only as temporary storage during the partitioning
    // Only leaf volumes have rooms, only non-leaf volumes have connecting halls
    union
    {
        Room room;
        Hall hall;
    };

    v3i voxelP;
    v3i sizeVoxels;
    u32 flags;
};

struct Cluster
{
    // TODO Determine what the bucket size should be so we have just one bucket most of the time
    BucketArray<StoredEntity> entityStorage;

    ClusterVoxelGrid voxelGrid;
    // TODO These should be actual entities (maybe just keep a minimal cache-friendly version here for fast iteration)
    Array<Room> rooms;
    Array<Hall> halls;

#if !RELEASE
    // Just for visualization
    BucketArray<aabb> debugVolumes;
#endif

    // TODO This probably go in a global mesh pool
    Array<Mesh> meshStore;

    bool populated;
};

inline u32 ClusterHash( const v3i& key, i32 tableSize );
inline u32 EntityHash( const u32& key, i32 tableSize );

// 'Thickness' of the sim region on each side of the origin cluster
// (in number of clusters)
const int SimExteriorHalfSize = 0;
const int SimRegionSizePerAxis = 2 * SimExteriorHalfSize + 1;

enum MeshGeneratorType
{
    GenRoom,

    GenCOUNT
};

struct World
{
    v3 pPlayer;
    f32 playerPitch;
    f32 playerYaw;

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
    // (We take the clusters we want to simulate, expand the entities stored there to their live version, and then store them back
    // when they're no longer active. Clusters around the player are always kept live)
    // TODO Always use the global coords value instead of the weird en-mass translation!
    // TODO Investigate what a good bucket size is
    BucketArray<LiveEntity> liveEntities;
    // Handles to stored entities to allow arbitrary entity cross-referencing even for entities that move
    // across clusters
    HashTable<u32, StoredEntity*, EntityHash> entityRefs;

    // Coordinates of the 'origin' cluster
    v3i originClusterP;
    v3i lastOriginClusterP;

    // TODO Should this be aligned and/or padded for cache niceness?
    IsoSurfaceSamplingCache* samplingCache;
    MeshPool* meshPools;

    MeshGeneratorJob generatorJobs[PLATFORM_MAX_JOBQUEUE_JOBS];
    i32 lastAddedJob;

    Array<v3> simClusterOffsets;
};



/// WORLD PARTITIONING ///

struct SectorParams
{
    f32 minVolumeRatio;
    f32 maxVolumeRatio;
    f32 minRoomSizeRatio;
    f32 maxRoomSizeRatio;
    i32 volumeSafeMarginSize;
    // Probability of keeping partitioning a volume once all its dimensions are smaller that the max size
    f32 volumeExtraPartitioningProbability;
};

SectorParams
CollectSectorParams( const v3i& clusterCoords )
{
    // TODO Retrieve the generation params for the cluster according to our future sector hierarchy
    // Just return some test values for now
    SectorParams result = {};
    result.minVolumeRatio = 0.4f;
    result.maxVolumeRatio = 0.5f;
    result.minRoomSizeRatio = 0.15f;
    result.maxRoomSizeRatio = 0.75f;
    result.volumeSafeMarginSize = 1;
    result.volumeExtraPartitioningProbability = 0.5f;

    ASSERT( result.minRoomSizeRatio > 0.f && result.minRoomSizeRatio < 1.f );
    ASSERT( result.maxRoomSizeRatio > 0.f && result.maxRoomSizeRatio < 1.f );
    ASSERT( result.volumeExtraPartitioningProbability >= 0.f && result.volumeExtraPartitioningProbability <= 1.f );

    return result;
}




#endif /* __WORLD_H__ */
