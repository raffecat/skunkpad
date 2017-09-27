/* Copyright (c) 2011, Andrew Towers <atowers@gmail.com>
** All rights reserved.
*/

// Block Index: memory blocks accessed by integer id.
// All blocks have the same size, fixed when the index is created.
// All ids are positive integers (i.e. greater than zero.)

struct block_index_nb { void* block; int id; };

block_index* block_index_create(size_t block_size, size_t grow_by);
void block_index_destroy(block_index* index);

// allocate a new block with a unique id.
// returns both the assigned id and the address of the new block.
// the returned pointer is valid until the next alloc or
// free request to this block index.
block_index_nb block_index_alloc(block_index* index);

// get the address of an allocated block from its id.
// returns a null pointer if the id is not allocated.
// the returned pointer is valid until the next alloc or
// free request to this block index.
void* block_index_get(block_index* index, int id);

// free the block associated with the specified id.
// the id becomes unallocated and can be re-used by a subsequent alloc.
void block_index_free(block_index* index, int id);


// In this implementation:
// All blocks are stored in a single consecutive allocation.
// The allocation is resized when full, copying all blocks.
// The block id is the index of the block in a packed array.
// The first block (index zero) contains the header.

struct block_index {
	size_t head_size;
	size_t block_size;
	unsigned int grow_by;
	unsigned int num_alloc;
	unsigned int num_used;
	unsigned int free_id;
};

// ensure header does not interfere with struct alignment.
union sys_align_u { double d; void* p; };
let SYS_ALIGN = sizeof(sys_align_u);
const size_t header_size = ((sizeof(block_index) + (SYS_ALIGN-1)) / SYS_ALIGN) * SYS_ALIGN;

block_index* block_index_create(size_t block_size, size_t grow_by)
{
	block_index* index;
	size_t alloc;
	// int sized blocks are required to store the free list.
	if (block_size < sizeof(int)) block_size = sizeof(int);
	// round up header size to a multiple of block size (alignment)
	alloc = (sizeof(block_index) + (block_size-1)) / block_size * block_size;
	
	// grow by around 4K each time unless specified.
	if (!grow_by) {
		grow_by = 4096 / block_size;
		if (grow_by < 8) grow_by = 8;
	}
	// allocate the 
	alloc = header_size + grow_by * block_size;
	index = malloc(alloc);
	

}

void block_index_destroy(block_index* index)
{
}

block_index_nb block_index_alloc(block_index* index)
{
}

void* block_index_get(block_index* index, int id)
{
	if (id > 0) {
		return (byte*)index + index->head_size + (id * index->block_size);
	}
}

void block_index_free(block_index* index, int id)
{
}
