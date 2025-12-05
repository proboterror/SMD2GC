#include "arena_allocator.h"

#include "stdio.h"

static uint8_t arena_buffer[ARENA_SIZE];
size_t arena_offset = 0;

uint8_t* arena_alloc(size_t size, size_t align)
{
	// Align offset upwards
	size_t offset = (arena_offset + align - 1) & ~(align - 1);

	if (offset + size > ARENA_SIZE)
	{
		printf("arena allocator out of space");
		return nullptr;
	}

	uint8_t* ptr = arena_buffer + offset;
	arena_offset = offset + size;

	return ptr;
}

void arena_reset()
{
	arena_offset = 0;
}

