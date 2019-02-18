#ifndef __WORLD_PARTITIONING_H__
#define __WORLD_PARTITIONING_H__ 

struct SectorParams
{
    r32 minVolumeSize;
    r32 maxVolumeSize;
};

SectorParams
CollectSectorParams( const v3i& clusterCoords )
{
    // TODO Just return some test values for now
    SectorParams result = {};
    result.minVolumeSize = 45;
    result.maxVolumeSize = 95;

    return result;
}


typedef struct Volume* VolumePtr;

struct Volume
{
    aabb bounds;
    VolumePtr leftChild, rightChild;
};

#endif /* __WORLD_PARTITIONING_H__ */
