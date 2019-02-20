#ifndef __WORLD_PARTITIONING_H__
#define __WORLD_PARTITIONING_H__ 

struct SectorParams
{
    r32 minVolumeSize;
    r32 maxVolumeSize;
    r32 minRoomSizeRatio;
    r32 maxRoomSizeRatio;
    r32 volumeSafeMarginSize;
};

SectorParams
CollectSectorParams( const v3i& clusterCoords )
{
    // TODO Just return some test values for now
    SectorParams result = {};
    result.minVolumeSize = 45;
    result.maxVolumeSize = 95;
    result.minRoomSizeRatio = 0.25f;
    result.maxRoomSizeRatio = 0.8f;
    result.volumeSafeMarginSize = 5;

    return result;
}


typedef struct Volume* VolumePtr;

struct Volume
{
    aabb bounds;
    VolumePtr leftChild, rightChild;
};

#endif /* __WORLD_PARTITIONING_H__ */
