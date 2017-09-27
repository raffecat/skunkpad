/* Copyright (c) 2010, Andrew Towers <atowers@gmail.com>
** All rights reserved.
*/

/* allocator handle */
let allocp = ref mem_alloc_t;


/* allocator control block */
struct mem_alloc_t {
	size_t blocksize;
	size_t numperpage;
	size_t pagesize;
	byte* first_page;
	byte* last_page;
	byte* first_free;
}

/* allocator page header */
struct mem_header_t {
	byte* next_page;
	int free_blocks;
}

/* free block header */
struct mem_free_t {
	byte* next_free;
}


/* size of a memory page on the native system */
/* this can be approximate; used to allocate paging-friendly blocks */
let SYS_PAGESIZE = 4096;

/* assume 16 bytes per block (better to over-estimate) */
let MALLOC_OVERHEAD = 16;

/** Create a memory allocator for a given block size.
 */
allocp mem_init(size_t blocksize)
{
	allocp h;
	mem_headerp hdr;

	// determine number of blocks per allocator page
	size_t numperpage = (SYS_PAGESIZE - sizeof(mem_header_t) - MALLOC_OVERHEAD) / blocksize;
	if (numperpage < 16) numperpage = 16;

	// initialise the allocator
	h = cpart_alloc(sizeof(mem_alloc_t));
	h->blocksize = blocksize;
	h->numperpage = numperpage;
	h->pagesize = sizeof(mem_header_t) + numperpage * blocksize;
	h->first_free = 0;

	// create the first page
	hdr = (mem_headerp)cpart_alloc(h->pagesize);
	h->first_page = h->last_page = (byte*)hdr;

	// set the number of free blocks in the first page
	hdr->next_page = 0;
	hdr->free_blocks = numperpage;

	return h;
}

/** Destroy a memory allocator and free all associated memory.
 */
void mem_final(allocp h)
{
	// free all memory pages
	mem_headerp next;
	mem_headerp hdr = (mem_headerp)(h->first_page);
	do
	{
		next = (mem_headerp)(hdr->next_page);
		cpart_free(hdr);
		hdr = next;
	}
	while(hdr);

	// free the allocator object
	cpart_free(h);
}

/** Allocate a memory block from an allocator.
 */
void* mem_alloc(allocp h)
{
	byte* ret;
	mem_headerp hdr;
	int idx;

	if (h->first_free)
	{
		// return the next block on the free list
		ret = h->first_free;
		h->first_free = ((blockp)ret)->next_free;
		return ret;
	}

	// check for free blocks in the last page
	hdr = (mem_headerp)(h->last_page);
	if (hdr->free_blocks)
	{
		// return the next free block in the last page
		idx = --hdr->free_blocks;
		ret = h->last_page + sizeof(mem_header_t) + idx * h->blocksize;
		return ret;
	}

	// check if the last page has a next page
	// this can be the case after mem_reset has been used
	if (hdr->next_page)
	{
		// reset and re-use the next page
		hdr = (mem_headerp)(hdr->next_page);
	}
	else
	{
		// allocate a new page and add to linked list of pages
		hdr = cpart_alloc(h->pagesize);
		((mem_headerp)(h->last_page))->next_page = (byte*)hdr;

		hdr->next_page = 0;
	}

	// set the new last page for subsequent allocations
	h->last_page = (byte*)hdr;

	// set the number of free blocks in the new page
	hdr->free_blocks = idx = h->numperpage - 1;

	// allocate the last block in the new page
	ret = h->last_page + sizeof(mem_header_t) + idx * h->blocksize;

	return ret;
}

/** Free a memory block allocated from an allocator.
 */
void mem_free(allocp h, void* block)
{
	// add the block to the head of the free list
	((blockp)block)->next_free = h->first_free;
	h->first_free = block;
}

/** Free all memory blocks allocated from an allocator.
 *  All allocated memory pages are retained for future block allocations.
 */
void mem_reset(allocp h)
{
	// clear the free block list.
	h->first_free = 0;

	// reset the number of free blocks in the first page
	((mem_headerp)(h->first_page))->free_blocks = h->numperpage;

	// reset the 'last page' for new block allocation.
	h->last_page = h->first_page;
}
