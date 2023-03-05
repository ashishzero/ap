#pragma once
#pragma once
#include "KrPlatform.h"


#ifndef M_ARENA_DEFAULT_CAP
#define M_ARENA_DEFAULT_CAP GB(1)
#endif

#ifndef M_ARENA_COMMIT_SIZE
#define M_ARENA_COMMIT_SIZE MB(32)
#endif

typedef struct M_Arena {
	umem Position;
	umem Reserved;
	umem Committed;
} M_Arena;

typedef struct M_ArenaInfo {
	u8 *First;
	u8 *Last;
	u8 *Position;
	u8 *Committed;
} M_ArenaInfo;

typedef struct M_ArenaTemp {
	M_Arena *Dst;
	umem     Pos;
} M_ArenaTemp;

M_Arena *   M_GetFallbackArena();
M_Arena *   M_ArenaAllocate(umem max_size, umem initial_size);
void        M_ArenaFree(M_Arena *arena);

void        M_ArenaReset(M_Arena *arena);
bool        M_ArenaEnsureCommit(M_Arena *arena, umem pos);
bool        M_ArenaSetPosition(M_Arena *arena, umem pos);
bool        M_ArenaPackPosition(M_Arena *arena, umem pos);
bool        M_ArenaAlignPosition(M_Arena *arena, u32 alignment);
void        M_ArenaQueryInfo(M_Arena *arena, M_ArenaInfo *info);

void *      M_PushSize(M_Arena *arena, umem size);
void *      M_PushSizeAligned(M_Arena *arena, umem size, u32 alignment);
M_ArenaTemp M_BeginTemporaryMemory(M_Arena *arena);
void *      M_PushSizeZero(M_Arena *arena, umem size);
void *      M_PushSizeAlignedZero(M_Arena *arena, umem size, u32 alignment);
void        M_PopSize(M_Arena *arena, umem size);
void        M_EndTemporaryMemory(M_ArenaTemp *temp);
void        M_ReleaseTemporaryMemory(M_ArenaTemp *temp);

void *      M_VirtualAlloc(void *ptr, size_t size);
bool        M_VirtualCommit(void *ptr, size_t size);
bool        M_VirtualDecommit(void *ptr, size_t size);
bool        M_VirtualFree(void *ptr, size_t size);
