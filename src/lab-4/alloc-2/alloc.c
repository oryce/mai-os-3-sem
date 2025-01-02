#define LIBRARY

#include "alloc.h"

#include <stdbool.h>
#include <unistd.h>

#define MAX_ORDER 30

typedef struct Block {
	/** Linked list pointer for the free list. */
	struct Block *next;
	/** The power-of-two order of this block. */
	size_t order;
} Block;

struct Allocator {
	/** Keep a free list for each order from 0..MAX_ORDER. */
	Block *free_lists[MAX_ORDER + 1];
	/** Start (base) of the entire memory region managed. */
	void *start;
	/** Total size of the memory region. */
	size_t size;
};

/** Returns the smallest power-of-two order that can fit 'size' bytes. */
static size_t get_order(size_t size) {
	size_t order = 0;
	size_t blockSize = 1;

	while (blockSize < size) {
		blockSize <<= 1;
		++order;
	}

	return order;
}

/** Computes 2^order. */
static size_t order_to_block_size(size_t order) { return (size_t)1 << order; }

/** Returns an offset within the allocator’s memory. */
static size_t get_offset(Allocator *allocator, void *ptr) {
	return (size_t)((char *)ptr - (char *)allocator->start);
}

/** Converts from offset to actual pointer in the memory region */
static void *offset_to_ptr(Allocator *allocator, size_t offset) {
	return (void *)((char *)allocator->start + offset);
}

/** Finds buddy of a given block. */
static void *find_buddy(Allocator *allocator, void *block_ptr, size_t order) {
	size_t offset = get_offset(allocator, block_ptr);
	size_t buddyOffset = offset ^ order_to_block_size(order);

	return offset_to_ptr(allocator, buddyOffset);
}

/** Insert a block into the free list for a given order. */
static void add_to_free_list(Allocator *allocator, Block *block, size_t order) {
	block->order = order;
	block->next = allocator->free_lists[order];

	allocator->free_lists[order] = block;
}

/** Remove a block from the free list. */
static void remove_from_free_list(Allocator *allocator, Block *block,
                                  size_t order) {
	Block *previous = NULL;
	Block *current = allocator->free_lists[order];

	while (current) {
		if (current == block) {
			// Found it, remove it from the list
			if (previous) {
				previous->next = current->next;
			} else {
				allocator->free_lists[order] = current->next;
			}
			return;
		}
		previous = current;
		current = current->next;
	}
}

Allocator *allocator_create(void *memory, size_t size) {
	if (!memory || size < sizeof(Allocator)) return NULL;

	Allocator *allocator = (Allocator *)memory;

	/* Initialize all free lists to NULL */
	for (int i = 0; i <= MAX_ORDER; i++) {
		allocator->free_lists[i] = NULL;
	}

	allocator->start = (char *)memory + sizeof(Allocator);
	allocator->size = size - sizeof(Allocator);

	size_t order = get_order(allocator->size);
	size_t exactBlockSize = order_to_block_size(order);

	if (exactBlockSize != allocator->size) {
		const char msg[] =
		    "warn: size of memory region is not a power of two, might cause "
		    "issues\n";
		write(STDOUT_FILENO, msg, sizeof(msg) - 1);
	}

	// Create one free block covering the entire buddy region.
	Block *block = (Block *)allocator->start;

	block->order = order;
	block->next = NULL;

	allocator->free_lists[order] = block;

	return allocator;
}

void allocator_destroy(Allocator *allocator) { (void)allocator; }

void *allocator_alloc(Allocator *allocator, size_t size) {
	if (!size) return NULL;

	// Figure out which order can fit `size`.
	size_t neededOrder = get_order(size + sizeof(Block));

	// Find a free block at or above that order, then split down if needed.
	size_t currentOrder = neededOrder;
	while (currentOrder <= MAX_ORDER &&
	       allocator->free_lists[currentOrder] == NULL) {
		currentOrder++;
	}
	if (currentOrder > MAX_ORDER) {
		// No block big enough.
		return NULL;
	}

	// Remove that block from the free list.
	Block *block = allocator->free_lists[currentOrder];
	remove_from_free_list(allocator, block, currentOrder);

	// Split down to the needed order
	while (currentOrder > neededOrder) {
		currentOrder--;

		size_t halfSize = order_to_block_size(currentOrder);
		// The buddy block starts half_size bytes after `block`.
		Block *buddy = (Block *)((char *)block + halfSize);
		// Put the buddy in the free list of the smaller order.
		add_to_free_list(allocator, buddy, currentOrder);
	}

	// |block| is now exactly big enough.
	block->order = neededOrder;
	block->next = NULL;

	return (void *)((char *)block + sizeof(Block));
}

void allocator_free(Allocator *allocator, void *memory) {
	if (!memory) return;

	Block *block = (Block *)((char *)memory - sizeof(Block));
	size_t order = block->order;

	// Coalesce as far as possible.
	while (order < MAX_ORDER) {
		void *buddyAddr = find_buddy(allocator, block, order);

		// Look for the buddy in the free list of this 'order'.
		bool foundBuddy = false;

		Block *previous = NULL;
		Block *current = allocator->free_lists[order];

		while (current) {
			if ((void *)current == buddyAddr) {
				foundBuddy = true;
				// Remove from free list.
				if (previous) {
					previous->next = current->next;
				} else {
					allocator->free_lists[order] = current->next;
				}
				break;
			}
			previous = current;
			current = current->next;
		}

		if (!foundBuddy) {
			break;  // can't coalesce
		}

		// Merge blocks.
		if (buddyAddr < (void *)block) {
			block = (Block *)buddyAddr;
		}

		++order;
	}

	// Add (possibly merged) block back.
	add_to_free_list(allocator, block, order);
}
