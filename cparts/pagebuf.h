#ifndef CPART_PAGEBUF
#define CPART_PAGEBUF


// Paged Buffer

typedef struct PagedBufferPage PagedBufferPage;
typedef struct PagedBuffer PagedBuffer;
typedef struct PagedBufferIter PagedBufferIter;
typedef struct PagedBufferIter2 PagedBufferIter2;

struct PagedBufferPage {
	PagedBufferPage* next;
	PagedBufferPage* prev;
	size_t size;
	size_t used;
	byte data[1];
};

struct PagedBuffer {
	PagedBufferPage* head;
	PagedBufferPage* tail;
	size_t pagesize;
};

struct PagedBufferIter {
	PagedBufferPage* page;
	size_t offset;
	PagedBuffer* pb;
};


#include "stream_proto.h"

struct PagedBufferIter2 {
	StreamIter iter;
	PagedBufferPage* page;
	PagedBuffer* pb;
};


// Paged Buffer API

void pagebuf_new(PagedBuffer* pb, size_t pagesize);
void pagebuf_new_iter(PagedBuffer* pb, PagedBufferIter* iter);
void pagebuf_new_iter2(PagedBuffer* pb, PagedBufferIter2* iter);

bool pagebuf_read(PagedBufferIter* iter, byte* data, size_t size);
void pagebuf_write(PagedBufferIter* iter, byte* data, size_t size);
void pagebuf_seek(PagedBufferIter* iter, size_t offset);
void pagebuf_skip(PagedBufferIter* iter, size_t offset);
void pagebuf_rewind(PagedBufferIter* iter, size_t offset);
size_t pagebuf_tell(PagedBufferIter* iter);

#endif
