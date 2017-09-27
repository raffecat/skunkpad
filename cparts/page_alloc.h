/* Copyright (c) 2010, Andrew Towers <atowers@gmail.com>
** All rights reserved.
*/

#ifndef CPART_PAGE_ALLOC
#define CPART_PAGE_ALLOC

// Page-based memory allocation.

// Pages are deemed to be correctly aligned for any use, and
// appropriately sized for the processor cache.

#define page_alloc_pgsize 4096

typedef struct page_alloc page_alloc;

void* page_alloc_new(page_alloc* allocator);


#endif
