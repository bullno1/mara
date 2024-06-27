#include "internal.h"

MARA_PRIVATE void*
mara_align_ptr(void* ptr, size_t alignment) {
	return (void*)(((intptr_t)ptr + (intptr_t)alignment - 1) & -(intptr_t)alignment);
}

MARA_PRIVATE void*
mara_alloc_from_chunk(mara_arena_chunk_t* chunk, size_t size, size_t alignment) {
	if (chunk != NULL) {
		char* bump_ptr = mara_align_ptr(chunk->bump_ptr, alignment);
		char* next_bump_ptr = bump_ptr + size;
		if (next_bump_ptr <= chunk->end) {
			chunk->bump_ptr = next_bump_ptr;
			return bump_ptr;
		} else {
			return NULL;
		}
	} else {
		return NULL;
	}
}

void*
mara_malloc(mara_allocator_t allocator, size_t size) {
	return mara_realloc(allocator, NULL, size);
}

void
mara_free(mara_allocator_t allocator, void* ptr) {
	mara_realloc(allocator, ptr, 0);
}

void*
mara_realloc(mara_allocator_t allocator, void* ptr, size_t new_size) {
	return allocator.fn(ptr, new_size, allocator.userdata);
}

void*
mara_arena_alloc_ex(mara_env_t* env, mara_arena_t* arena, size_t size, size_t alignment) {
	void* mem = mara_alloc_from_chunk(arena->current_chunk, size, alignment);
	if (MARA_EXPECT(mem != NULL)) {
		return mem;
	} else {
		size_t configured_chunk_size = env->options.alloc_chunk_size;
		size_t required_chunk_size = sizeof(mara_arena_chunk_t) + size;
		size_t chunk_size = mara_max(configured_chunk_size, required_chunk_size);

		mara_arena_chunk_t* new_chunk;
		{
			mara_arena_chunk_t* free_chunk = env->free_chunks;
			if (
				free_chunk != NULL
				&& (size_t)(free_chunk->end - (char*)free_chunk) >= chunk_size
			) {
				new_chunk = free_chunk;
				env->free_chunks = new_chunk->next;
			} else {
				new_chunk = mara_malloc(env->options.allocator, chunk_size);
				mara_assert(new_chunk != NULL, "Out of memory");
				new_chunk->end = (char*)new_chunk + chunk_size;
			}
		}

		new_chunk->bump_ptr = new_chunk->begin;
		new_chunk->next = arena->current_chunk;
		arena->current_chunk = new_chunk;
		return mara_alloc_from_chunk(new_chunk, size, alignment);
	}
}

void*
mara_arena_alloc(mara_env_t* env, mara_arena_t* arena, size_t size) {
	return mara_arena_alloc_ex(env, arena, size, _Alignof(MARA_ALIGN_TYPE));
}

mara_arena_snapshot_t
mara_arena_snapshot(mara_env_t* env, mara_arena_t* arena) {
	(void)env;
	mara_arena_chunk_t* current_chunk = arena->current_chunk;
	return (mara_arena_snapshot_t){
		.chunk = current_chunk,
		.bump_ptr = current_chunk != NULL ? current_chunk->bump_ptr : NULL,
	};
}

void
mara_arena_restore(mara_env_t* env, mara_arena_t* arena, mara_arena_snapshot_t snapshot) {
	mara_arena_chunk_t* chunk = arena->current_chunk;
	while (chunk != snapshot.chunk) {
		mara_arena_chunk_t* next = chunk->next;
		chunk->next = env->free_chunks;
		env->free_chunks = chunk;
		chunk = next;
	}

	if (chunk != NULL) {
		chunk->bump_ptr = snapshot.bump_ptr;
	}

	arena->current_chunk = chunk;
}

void
mara_arena_reset(mara_env_t* env, mara_arena_t* arena) {
	mara_arena_restore(env, arena, (mara_arena_snapshot_t){ 0 });
}
