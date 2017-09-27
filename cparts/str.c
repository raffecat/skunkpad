#include "defs.h"
#include "str.h"


// StringRef

const stringref str_empty = { 0, "" };

bool str_equal(stringref s1, stringref s2) {
	return s1.size == s2.size && memcmp(s1.data, s2.data, s1.size) == 0;
}

stringref str_substr(stringref s, size_t first, size_t end) {
	if (end > s.size) end = s.size;
	if (first < end) { // NB. first must therefore be in bounds.
		stringref ret; { ret.size = end - first; ret.data = s.data + first; }
		return ret;
	}
	return str_empty;
}

size_t str_find(stringref s, stringref sub, size_t init) {
	if (sub.size && s.size >= sub.size) {
		size_t i, last = s.size - sub.size;
		// TODO: KMP with limited table?
		for (i=(init < last ? init : last); i <= last; ++i) {
			if (s.data[i] == sub.data[0] &&
				memcmp(s.data+i, sub.data, sub.size) == 0)
					return i;
		}
	}
	return -1;
}

size_t str_rfind(stringref s, stringref sub, size_t init) {
	if (sub.size && s.size >= sub.size) {
		size_t i, last = s.size - sub.size;
		// TODO: KMP with limited table?
		for (i=(init < last ? init : last)+1; i > 0; ) {
			--i;
			if (s.data[i] == sub.data[0] &&
				memcmp(s.data+i, sub.data, sub.size) == 0)
					return i;
		}
	}
	return -1;
}


// String

#define STRSIZE (offsetof(struct string_s, data)+1)

string str_alloc(size_t size) {
	string s = cpart_alloc(STRSIZE + size);
	s->refs = 1;
	s->size = size;
	s->data[size] = '\0'; // for C libraries.
	return s;
}

string str_create(stringref ref) {
	string s = str_alloc(ref.size);
	memcpy(s->data, ref.data, ref.size);
	return s;
}

string str_create_c(const char* cstr) {
	size_t size = strlen(cstr);
	string s = str_alloc(size);
	memcpy(s->data, cstr, size);
	return s;
}

string str_create_cl(const char* data, size_t size) {
	string s = str_alloc(size);
	memcpy(s->data, data, size);
	return s;
}

string str_concat(stringref s1, stringref s2) {
	size_t size = s1.size + s2.size;
	string s = str_alloc(size);
	memcpy(s->data, s1.data, s1.size);
	memcpy(s->data + s1.size, s2.data, s2.size);
	return s;
}

int str_free_internal(string s) {
	assert(s != 0 && s->refs == 0);
	free(s);
	return 0; // for str_release macro; value not important.
}


// StringBuf

string stringbuf_alloc_internal(size_t size) {
	string s = cpart_alloc(STRSIZE + size);
	s->refs = 1;
	s->size = 0;
	return s;
}

static void stringbuf_grow(stringbuf* buf, size_t req) {
	string s;
	buf->alloc = req + 512;
	s = cpart_alloc(STRSIZE + req + 512);
	s->refs = 1;
	s->size = buf->s->size;
	memcpy(s->data, buf->s->data, buf->s->size);
	free(buf->s);
	buf->s = s;
}

void stringbuf_append(stringbuf* buf, stringref s) {
	size_t req = buf->s->size + s.size;
	if (req > buf->alloc) stringbuf_grow(buf, req);
	{ char* dest = buf->s->data + buf->s->size;
	buf->s->size = req; // before call is better.
	memcpy(dest, s.data, s.size); }
}

char* stringbuf_reserve(stringbuf* buf, size_t size) {
	size_t req = buf->s->size + size;
	if (req > buf->alloc) stringbuf_grow(buf, req);
	return buf->s->data + buf->s->size;
}

void stringbuf_commit(stringbuf* buf, size_t size) {
	buf->s->size += size;
}

void stringbuf_truncate(stringbuf* buf, size_t size) {
	if (size < buf->s->size)
		buf->s->size = size;
}

string stringbuf_finish(stringbuf* buf) {
	buf->s->data[buf->s->size] = '\0'; // for C libraries.
	return buf->s;
}
