#include "defs.h"
#include "pagebuf.h"


static PagedBufferPage* append_page(PagedBuffer* pb)
{
	PagedBufferPage* page = cpart_alloc(pb->pagesize);
	page->next = 0;
	page->prev = pb->tail;
	page->size = pb->pagesize - offsetof(PagedBufferPage, data);
	page->used = 0;
	if (pb->tail)
		pb->tail->next = page;
	pb->tail = page;
	return page;
}

void pagebuf_new(PagedBuffer* pb, size_t pagesize)
{
	pb->pagesize = pagesize;
	pb->tail = 0;
	pb->head = append_page(pb); // must have one page.
}

void pagebuf_new_iter(PagedBuffer* pb, PagedBufferIter* iter)
{
	iter->page = pb->head;
	iter->offset = 0;
	iter->pb = pb;
}

bool pagebuf_read(PagedBufferIter* iter, byte* data, size_t size)
{
	size_t remain = iter->page->used - iter->offset;
	for (;;) {
		if (size > remain) {
			// copy page data to the output.
			memcpy(data, iter->page->data + iter->offset, remain);
			size -= remain;
			// advance to the next page.
			if (iter->page->next) {
				iter->page = iter->page->next;
				iter->offset = 0;
				remain = iter->page->used;
			} else {
				iter->offset = iter->page->used;
				return false; // end of buffer.
			}
		} else {
			// copy all remaining data.
			memcpy(data, iter->page->data + iter->offset, size);
			iter->offset += size;
			break; // all data read.
		}
	}
	return true;
}

void pagebuf_write(PagedBufferIter* iter, byte* data, size_t size)
{
	size_t remain = iter->page->size - iter->offset;
	for (;;) {
		if (size > remain) {
			// copy source data to the current page.
			iter->page->used = iter->page->size;
			memcpy(iter->page->data + iter->offset, data, remain);
			size -= remain;
			// advance to the next page.
			iter->offset = 0;
			if (iter->page->next) {
				iter->page = iter->page->next;
			} else {
				iter->page = append_page(iter->pb);
			}
			remain = iter->page->size;
		}
		else {
			// write the rest to this page.
			memcpy(iter->page->data + iter->offset, data, size);
			iter->offset += size;
			if (iter->page->used < iter->offset)
				iter->page->used = iter->offset;
			break; // all data written.
		}
	}
}

void pagebuf_skip(PagedBufferIter* iter, size_t offset)
{
	for (;;) {
		size_t remain = iter->page->used - iter->offset;
		if (offset > remain) {
			offset -= remain;
			// advance to the next page.
			if (iter->page->next) {
				iter->page = iter->page->next;
				iter->offset = 0;
			} else {
				iter->offset = iter->page->used;
				return; // end of buffer.
			}	
		} else {
			iter->offset += offset;
			break; // found correct position.
		}
	}
}

void pagebuf_rewind(PagedBufferIter* iter, size_t offset)
{
	for (;;) {
		if (offset > iter->offset) {
			offset -= iter->offset;
			// rewind to previous page.
			if (iter->page->prev) {
				iter->page = iter->page->prev;
				iter->offset = iter->page->used;
			} else {
				iter->offset = 0;
				return; // start of buffer.
			}
		} else {
			iter->offset -= offset;
			break; // found correct position.
		}
	}
}

size_t pagebuf_tell(PagedBufferIter* iter)
{
	size_t ofs = iter->offset;
	PagedBufferPage* page = iter->page->prev;
	while (page) {
		ofs += iter->page->size; page = page->prev;
	}
	return ofs;
}

void pagebuf_seek(PagedBufferIter* iter, size_t offset)
{
	size_t pos = pagebuf_tell(iter);
	if (pos < offset) pagebuf_skip(iter, offset - pos);
	else pagebuf_rewind(iter, pos - offset);
}


// -------------------------------------------------------------
// new iterator

bool pagebuf_more_func(StreamIter* iter)
{
	return false;
}

void pagebuf_new_iter2(PagedBuffer* pb, PagedBufferIter2* iter)
{
	iter->iter.data = pb->head->data;
	iter->iter.remain = pb->head->size;
	iter->iter.more = pagebuf_more_func;
	iter->page = pb->head;
	iter->pb = pb;
}
