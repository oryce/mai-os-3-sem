#include "fallback.h"

#include <sys/mman.h>

struct Allocator {
	/** Some data to silence the compiler's empty
	 * struct warning. */
	char marker;
};

static Allocator DUMMY_ALLOCATOR = (Allocator){'\0'};

typedef struct Block {
	size_t size;
} Block;

Allocator* fb_allocator_create(void* memory, size_t size) {
	return &DUMMY_ALLOCATOR;
}

void fb_allocator_destroy(Allocator* allocator) {
	// no-op
}

void* fb_allocator_alloc(Allocator* allocator, size_t size) {
	Block* block =
	    (Block*)mmap(NULL, sizeof(Block) + size, PROT_READ | PROT_WRITE,
	                 MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
	if (block == MAP_FAILED) return NULL;

	block->size = size;

	return (char*)block + sizeof(Block);
}

void fb_allocator_free(Allocator* allocator, void* memory) {
	if (!memory) return;

	Block* block = (Block*)((char*)memory - sizeof(Block));

	// Free allocated memory for the block.
	munmap(block, block->size + sizeof(Block));
}
