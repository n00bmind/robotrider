#ifndef __WORLD_PARTITIONING_H__
#define __WORLD_PARTITIONING_H__ 


// NOTE Everything related to maze generation and connectivity is measured in voxel units
const r32 VoxelSizeMeters = 1.f;
const u32 VoxelsPerClusterAxis = 512;

struct SectorParams
{
    i32 minVolumeSize;
    i32 maxVolumeSize;
    r32 minRoomSizeRatio;
    r32 maxRoomSizeRatio;
    i32 volumeSafeMarginSize;
    // Probability of keeping partitioning a volume once all its dimensions are smaller that the max size
    r32 volumeExtraPartitioningProbability;
};

SectorParams
CollectSectorParams( const v3i& clusterCoords )
{
    // TODO Just return some test values for now
    SectorParams result = {};
    result.minVolumeSize = 100;
    result.maxVolumeSize = 250;
    result.minRoomSizeRatio = 0.15f;
    result.maxRoomSizeRatio = 0.75f;
    result.volumeSafeMarginSize = 1;
    result.volumeExtraPartitioningProbability = 0.5f;

    ASSERT( result.minRoomSizeRatio > 0.f && result.minRoomSizeRatio < 1.f );
    ASSERT( result.maxRoomSizeRatio > 0.f && result.maxRoomSizeRatio < 1.f );
    ASSERT( result.volumeExtraPartitioningProbability >= 0.f && result.volumeExtraPartitioningProbability <= 1.f );

    return result;
}


typedef struct Volume* VolumePtr;

struct Volume
{
    aabbi bounds;                       // Inclusive
    VolumePtr leftChild, rightChild;
};

#endif /* __WORLD_PARTITIONING_H__ */
