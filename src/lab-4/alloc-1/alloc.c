#define LIBRARY

#include "alloc.h"

#include <stdbool.h>
#include <string.h>

static const size_t MIN_SIZE = 16;

typedef struct Block {
	size_t size;
	struct Block *next;
	bool used;
} Block;

struct Allocator {
	/** Pointer to the first free block. */
	Block *head;
	/** Total memory available to the allocator. */
	size_t size;
};

Allocator *allocator_create(void *memory, size_t size) {
	if (!memory || size < sizeof(Allocator)) return NULL;

	Allocator *allocator = (Allocator *)memory;

	allocator->head = (Block *)((char *)memory + sizeof(Allocator));
	allocator->size = size;

	// Initialize head (first) block.
	allocator->head->size = size - sizeof(Allocator) - sizeof(Block);
	allocator->head->next = NULL;
	allocator->head->used = false;

	return allocator;
}

void allocator_destroy(Allocator *allocator) {
	if (!allocator) return;
	memset(allocator, 0, allocator->size);
}

void *allocator_alloc(Allocator *allocator, size_t size) {
	if (!allocator || !size) return NULL;

	// Find the most suitable block (closest to |size|.)
	Block *current = allocator->head, *previous = NULL;
	Block *best = NULL, *prevBest = NULL;

	while (current) {
		if (!current->used && current->size >= size) {
			if (!best || current->size < best->size) {
				best = current;
				prevBest = previous;
			}
		}

		previous = current;
		current = current->next;
	}

	if (!best) {
		// No block found.
		return NULL;
	}

	// Remove 'best' from the free list.
	if (prevBest) {
		prevBest->next = best->next;
	} else {
		allocator->head = best->next;
	}

	// If it makes sense to insert a new block (i.e., if we have some space),
	// insert one.
	size_t leftover = best->size - size;

	if (leftover >= MIN_SIZE + sizeof(Block)) {
		Block *newBlock = (Block *)((char *)best + sizeof(Block) + size);
		newBlock->size = leftover - sizeof(Block);
		newBlock->used = false;

		// Insert the block at the top of the list.
		newBlock->next = allocator->head;
		allocator->head = newBlock;
	}

	best->size = size;
	best->used = true;

	return (char *)best + sizeof(Block);
}

void allocator_free(Allocator *allocator, void *memory) {
	if (!allocator || !memory) return;

	Block *block = (Block *)((char *)memory - sizeof(Block));

	// Insert the block at the top of the list.
	block->next = allocator->head;
	block->used = false;
	allocator->head = block;

	// Merge blocks that are right next to each other.
	Block *current = allocator->head;

	while (current && current->next) {
		Block *next = current->next;

		bool canMerge =
		    (Block *)((char *)current + sizeof(Block) + current->size) == next;

		if (canMerge) {
			current->size += sizeof(Block) + next->size;
			current->next = next->next;
		} else {
			current = next;
		}
	}
}
