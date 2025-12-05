#pragma once

#include <stddef.h>
#include <stdint.h>

#define ARENA_SIZE (4 * 1024)

uint8_t* arena_alloc(size_t size, size_t align = 4);
void arena_reset();
