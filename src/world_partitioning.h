#ifndef __WORLD_PARTITIONING_H__
#define __WORLD_PARTITIONING_H__ 

typedef struct Volume* VolumePtr;

struct Volume
{
    aabb bounds;
    VolumePtr leftChild, rightChild;
};

#endif /* __WORLD_PARTITIONING_H__ */
