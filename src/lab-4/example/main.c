#include <dlfcn.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <time.h>

#include "alloc.h"
#include "bootleg_stdio.h"
#include "fallback.h"

static const size_t MEMORY_SIZE =
    1024 * 1024 + 264 /* sizeof(BuddyAllocator) */;

static pfnAllocatorCreate allocatorCreate;
static pfnAllocatorDestroy allocatorDestroy;
static pfnAllocatorAlloc allocatorAlloc;
static pfnAllocatorFree allocatorFree;

static bool load_symbols(void* library) {
	allocatorCreate = (pfnAllocatorCreate)dlsym(library, "allocator_create");
	if (!allocatorCreate) {
		blg_perrorf("can't find `allocator_create`\n");
		return false;
	}

	allocatorDestroy = (pfnAllocatorDestroy)dlsym(library, "allocator_destroy");
	if (!allocatorDestroy) {
		blg_perrorf("can't find `allocator_destroy`\n");
		return false;
	}

	allocatorAlloc = (pfnAllocatorAlloc)dlsym(library, "allocator_alloc");
	if (!allocatorAlloc) {
		blg_perrorf("can't find `allocator_alloc`\n");
		return false;
	}

	allocatorFree = (pfnAllocatorFree)dlsym(library, "allocator_free");
	if (!allocatorFree) {
		blg_perrorf("can't find `allocator_free`\n");
		return false;
	}

	return true;
}

static size_t rand_size() {
	static const size_t BLOCK_SIZE = 1024;

	return rand() % BLOCK_SIZE + 1;
}

static void test_allocator(Allocator* allocator) {
	static const size_t BLOCK_COUNT = 512;

	void* blocks[BLOCK_COUNT];

	clock_t startAlloc = clock();

	for (size_t i = 0; i != BLOCK_COUNT; ++i) {
		size_t size = rand_size();

		blocks[i] = allocatorAlloc(allocator, size);
		// blg_printf("alloc(%zu) = %p\n", size, blocks[i]);

		if (!blocks[i]) {
			blg_perrorf("can't allocate block\n");
		}
	}

	clock_t endAlloc = clock();
	clock_t startFree = clock();

	// Free half of the blocks randomly.
	for (size_t i = 0; i != BLOCK_COUNT / 2; ++i) {
		size_t idx = rand() % BLOCK_COUNT;

		allocatorFree(allocator, blocks[idx]);
		// blg_printf("free(%p)\n", blocks[idx]);

		blocks[idx] = NULL;
	}

	clock_t endFree = clock();

	// Try to allocate on the freed blocks.
	for (size_t i = 0; i != BLOCK_COUNT; ++i) {
		if (blocks[i]) continue;

		size_t size = rand_size();

		blocks[i] = allocatorAlloc(allocator, size);
		// blg_printf("alloc(%zu) = %p\n", size, blocks[i]);

		if (!blocks[i]) {
			blg_perrorf("can't allocate block\n");
		}
	}

	blg_printf("alloc took %fs\n",
	           (double)(endAlloc - startAlloc) / CLOCKS_PER_SEC);
	blg_printf("free took %fs\n",
	           (double)(endFree - startFree) / CLOCKS_PER_SEC);

	for (size_t i = 0; i != BLOCK_COUNT; ++i) {
		allocatorFree(allocator, blocks[i]);
	}
}

static void stress_allocator(Allocator* allocator) {
	static const size_t ITERATIONS = 200000;
	static const size_t BLOCK_COUNT = 20000;

	void* blocks[BLOCK_COUNT];

	for (size_t i = 0; i != BLOCK_COUNT; ++i) {
		blocks[i] = NULL;
	}

	clock_t start = clock();

	// Keep track of how many blocks are in use and randomly decide,
	// whether to allocate or free.
	size_t used = 0;

	for (size_t op = 0; op < ITERATIONS; op++) {
		bool alloc = true;

		if (used == 0) {
			alloc = true;
		} else if (used == BLOCK_COUNT) {
			alloc = false;
		} else {
			alloc = rand() % 2;
		}

		if (alloc) {
			size_t idx;
			do {
				idx = rand() % BLOCK_COUNT;
			} while (blocks[idx] != NULL);

			size_t size = rand_size();
			blocks[idx] = allocatorAlloc(allocator, size);

			if (blocks[idx]) {
				++used;
			} else {
				// Allocation failed
			}
		} else {
			size_t idx;
			do {
				idx = rand() % BLOCK_COUNT;
			} while (blocks[idx] == NULL);

			allocatorFree(allocator, blocks[idx]);
			blocks[idx] = NULL;

			--used;
		}
	}

	clock_t end = clock();

	double elapsed = (double)(end - start) / CLOCKS_PER_SEC;
	blg_printf("stress test took %fs\n", elapsed);

	for (size_t i = 0; i != BLOCK_COUNT; ++i) {
		allocatorFree(allocator, blocks[i]);
	}
}

static int cleanup(int retVal, void* library, void* memory,
                   Allocator* allocator) {
	if (allocator) allocatorDestroy(allocator);
	if (memory) munmap(memory, MEMORY_SIZE);
	if (library) dlclose(library);

	return retVal;
}

int main(int argc, char* argv[]) {
	void* library = NULL;
	void* memory = NULL;
	Allocator* allocator = NULL;

	library = dlopen(argv[1], RTLD_LOCAL | RTLD_NOW);
	bool libraryLoaded = false;

	// Try to load the allocator library from argv[1].
	if (argc > 1 && library) {
		libraryLoaded = load_symbols(library);
	}

	// If we can't load one of the symbols, use fallbacks.
	if (!libraryLoaded) {
		blg_printf("can't load allocator library, using fallbacks");

		allocatorCreate = fb_allocator_create;
		allocatorDestroy = fb_allocator_destroy;
		allocatorAlloc = fb_allocator_alloc;
		allocatorFree = fb_allocator_free;
	}

	// Create a 1 MiB memory region.
	memory = mmap(NULL, MEMORY_SIZE, PROT_READ | PROT_WRITE,
	              MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

	if (memory == MAP_FAILED) {
		blg_perrorf("can't create memory region\n");
		return cleanup(1, library, memory, allocator);
	}

	// Create an allocator.
	allocator = allocatorCreate(memory, MEMORY_SIZE);

	if (!allocator) {
		blg_perrorf("can't create allocator\n");
		return cleanup(2, library, memory, allocator);
	}

	// Test the allocator.
	srand(123456);

	test_allocator(allocator);
	stress_allocator(allocator);

	return cleanup(0, library, memory, allocator);
}
