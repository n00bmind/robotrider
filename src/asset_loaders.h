#ifndef __ASSET_LOADERS_H__
#define __ASSET_LOADERS_H__ 

#if NON_UNITY_BUILD
#include "wfc.h"
#endif

Texture LoadTexture( const char* path, bool flipVertically, bool filtered = true, int desiredChannels = 0 );
Array<WFC::Spec> LoadWFCVars( const char* path, MemoryArena* arena, const TemporaryMemory& tempMemory );
Mesh LoadOBJ( const char* path, MemoryArena* arena, const TemporaryMemory& tmpMemory, const m4& appliedTransform = M4Identity );

#endif /* __ASSET_LOADERS_H__ */
