#include "KrMemory.h"

#include <string.h>

static M_Arena FallbackArena;

M_Arena *M_GetFallbackArena() {
	return &FallbackArena;
}

M_Arena *M_ArenaAllocate(umem max_size, umem initial_size) {
	if (!max_size)
		max_size = M_ARENA_DEFAULT_CAP;

	u8 *mem  = (u8 *)M_VirtualAlloc(0, max_size);
	if (mem) {
		umem commit_size = AlignUp2(initial_size, M_ARENA_COMMIT_SIZE);
		commit_size      = Clamp(M_ARENA_COMMIT_SIZE, max_size, commit_size);
		if (M_VirtualCommit(mem, commit_size)) {
			M_Arena *arena   = (M_Arena *)mem;
			arena->Position  = sizeof(M_Arena);
			arena->Reserved  = max_size;
			arena->Committed = commit_size;
			return arena;
		}
		M_VirtualFree(mem, max_size);
	}
	return nullptr;
}

void M_ArenaFree(M_Arena *arena) {
	M_VirtualFree(arena, arena->Reserved);
}

void M_ArenaReset(M_Arena *arena) {
	arena->Position = sizeof(M_Arena);
}

bool M_ArenaEnsureCommit(M_Arena *arena, umem pos) {
	if (pos <= arena->Committed) {
		return true;
	}

	pos = Max(pos, M_ARENA_COMMIT_SIZE);
	u8 *mem = (u8 *)arena;

	umem committed = AlignUp2(pos, M_ARENA_COMMIT_SIZE);
	committed = Min(committed, arena->Reserved);

	if (M_VirtualCommit(mem + arena->Committed, committed - arena->Committed)) {
		arena->Committed = committed;
		return true;
	}
	return false;
}

bool M_ArenaSetPosition(M_Arena *arena, umem pos) {
	if (M_ArenaEnsureCommit(arena, pos)) {
		arena->Position = pos;
		return true;
	}
	return false;
}

bool M_ArenaPackPosition(M_Arena *arena, umem pos) {
	if (M_ArenaSetPosition(arena, pos)) {
		umem committed = AlignUp2(pos, M_ARENA_COMMIT_SIZE);
		committed = Clamp(M_ARENA_COMMIT_SIZE, arena->Reserved, committed);

		u8 *mem = (u8 *)arena;
		if (committed < arena->Committed) {
			if (M_VirtualDecommit(mem + committed, arena->Committed - committed))
				arena->Committed = committed;
		}
		return true;
	}
	return false;
}

bool M_ArenaAlignPosition(M_Arena *arena, u32 alignment) {
	u8 *mem     = (u8 *)arena + arena->Position;
	u8 *aligned = (u8 *)AlignUp2((umem)mem, alignment);
	umem pos    = arena->Position + (aligned - mem);
	if (M_ArenaSetPosition(arena, pos))
		return true;
	return false;
}

void M_ArenaQueryInfo(M_Arena *arena, M_ArenaInfo *info) {
	info->First     = (u8 *)arena;
	info->Last      = info->First + arena->Reserved;
	info->Position  = info->First + arena->Position;
	info->Committed = info->First + arena->Committed;
}

void *M_PushSize(M_Arena *arena, umem size) {
	u8 *mem  = (u8 *)arena + arena->Position;
	umem pos = arena->Position + size;
	if (M_ArenaSetPosition(arena, pos))
		return mem;
	return 0;
}

void *M_PushSizeAligned(M_Arena *arena, umem size, u32 alignment) {
	if (M_ArenaAlignPosition(arena, alignment))
		return M_PushSize(arena, size);
	return nullptr;
}

M_ArenaTemp M_BeginTemporaryMemory(M_Arena *arena) {
	return (M_ArenaTemp) {
		.Dst = arena,
		.Pos = arena->Position
	};
}

void *M_PushSizeZero(M_Arena *arena, umem size) {
	void *result = M_PushSize(arena, size);
	memset(result, 0, size);
	return result;
}

void *M_PushSizeAlignedZero(M_Arena *arena, umem size, u32 alignment) {
	void *result = M_PushSizeAligned(arena, size, alignment);
	memset(result, 0, size);
	return result;
}

void M_PopSize(M_Arena *arena, umem size) {
	size_t pos = arena->Position - size;
	Assert(pos >= sizeof(M_Arena) && pos <= arena->Reserved);
	M_ArenaSetPosition(arena, pos);
}

void M_EndTemporaryMemory(M_ArenaTemp *temp) {
	temp->Dst->Position = temp->Pos;
}

void M_ReleaseTemporaryMemory(M_ArenaTemp *temp) {
	M_ArenaPackPosition(temp->Dst, temp->Pos);
}

//
//
//

#if M_PLATFORM_WINDOWS == 1
#pragma warning(push)
#pragma warning(disable : 5105)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#pragma warning(pop)

void *M_VirtualAlloc(void *ptr, size_t size) {
	return VirtualAlloc(ptr, size, MEM_RESERVE, PAGE_READWRITE);
}

bool M_VirtualCommit(void *ptr, size_t size) {
	return VirtualAlloc(ptr, size, MEM_COMMIT, PAGE_READWRITE) != NULL;
}

bool M_VirtualDecommit(void *ptr, size_t size) {
	return VirtualFree(ptr, size, MEM_DECOMMIT);
}

bool M_VirtualFree(void *ptr, size_t size) {
	return VirtualFree(ptr, 0, MEM_RELEASE);
}

#endif

#if M_PLATFORM_LINUX == 1 || M_PLATFORM_MAC == 1
#include <sys/mman.h>
#include <stdlib.h>

void *M_VirtualAlloc(void *ptr, size_t size) {
	void *result = mmap(ptr, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (result == MAP_FAILED)
		return NULL;
	return result;
}

bool M_VirtualCommit(void *ptr, size_t size) {
	return mprotect(ptr, size, PROT_READ | PROT_WRITE) == 0;
}

bool M_VirtualDecommit(void *ptr, size_t size) {
	return mprotect(ptr, size, PROT_NONE) == 0;
}

bool M_VirtualFree(void *ptr, size_t size) {
	return munmap(ptr, size) == 0;
}

#endif
