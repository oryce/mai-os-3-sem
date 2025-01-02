#pragma once

#include <stddef.h>

#ifdef _MSC_VER
  #define EXPORT __declspec(dllexport)
#else
  #define EXPORT
#endif

struct Allocator;

typedef struct Allocator Allocator;

#ifdef LIBRARY
  EXPORT Allocator *allocator_create(void *memory, size_t size);

  EXPORT void allocator_destroy(Allocator *allocator);

  EXPORT void *allocator_alloc(Allocator *allocator, size_t size);

  EXPORT void allocator_free(Allocator *allocator, void *memory);
#endif

typedef Allocator *(*pfnAllocatorCreate)(void *memory, size_t size);

typedef void (*pfnAllocatorDestroy)(Allocator *allocator);

typedef void *(*pfnAllocatorAlloc)(Allocator *allocator, size_t size);

typedef void (*pfnAllocatorFree)(Allocator *allocator, void *memory);
