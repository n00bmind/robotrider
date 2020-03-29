#ifndef __WORLD_PARTITIONING_H__
#define __WORLD_PARTITIONING_H__ 


// NOTE Everything related to maze generation and connectivity is measured in voxel units
const r32 VoxelSizeMeters = 1.f;
const i32 VoxelsPerClusterAxis = 256;

struct SectorParams
{
    r32 minVolumeRatio;
    r32 maxVolumeRatio;
    r32 minRoomSizeRatio;
    r32 maxRoomSizeRatio;
    i32 volumeSafeMarginSize;
    // Probability of keeping partitioning a volume once all its dimensions are smaller that the max size
    r32 volumeExtraPartitioningProbability;
};

SectorParams
CollectSectorParams( const v3i& clusterCoords )
{
    // TODO Retrieve the generation params for the cluster according to our future sector hierarchy
    // Just return some test values for now
    SectorParams result = {};
    result.minVolumeRatio = 0.2f;
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


struct Room
{
    v3u voxelP;
    v3u sizeVoxels;
};

struct Hall
{
    v3u startP;
    v3u endP;
    // Encoded as 2 bits per axis, first axis is LSB
    u8 axisOrder;
};

struct BinaryVolume
{
    BinaryVolume* leftChild;
    BinaryVolume* rightChild;

    union
    {
        Room room;
        Hall hall;
    };

    v3u voxelP;
    v3u sizeVoxels;
};

#endif /* __WORLD_PARTITIONING_H__ */
