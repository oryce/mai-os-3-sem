#pragma once

#include "alloc.h"

Allocator* fb_allocator_create(void* memory, size_t size);

void fb_allocator_destroy(Allocator* allocator);

void* fb_allocator_alloc(Allocator* allocator, size_t size);

void fb_allocator_free(Allocator* allocator, void* memory);
