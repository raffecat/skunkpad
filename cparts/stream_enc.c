// Encoding for streams.

#include "defs.h"
#include "stream_enc.h"
#include "stream_proto.h"

struct StreamIter;

bool stream_read(struct StreamIter* iter, byte* dest, size_t size);
bool stream_write(struct StreamIter* iter, const byte* src, size_t size);

byte stream_read_byte(struct StreamIter* iter);
void stream_write_byte(struct StreamIter* iter, byte value);

int stream_read_int(struct StreamIter* iter);
void stream_write_int(struct StreamIter* iter, int value);

unsigned int stream_read_uint(struct StreamIter* iter);
void stream_write_uint(struct StreamIter* iter, unsigned int value);


bool stream_read(StreamIter* iter, byte* dest, size_t size)
{
	for (;;) {
		if (size <= iter->remain) {
			// all remaining data is in the current buffer.
			const byte* from = iter->data;
			iter->remain -= size;
			iter->data += size;
			memcpy(dest, from, size);
			return true;
		} else {
			// read remaining buffer and advance destination.
			byte* to = dest;
			size -= iter->remain;
			dest += iter->remain;
			memcpy(to, iter->data, iter->remain);
			// get some more data.
			if (!iter->more(iter))
				return false;
		}
	}
}

bool stream_write(StreamIter* iter, const byte* src, size_t size)
{
	for (;;) {
		if (size <= iter->remain) {
			// all remaining data will fit in current buffer.
			byte* to = iter->data;
			iter->remain -= size;
			iter->data += size;
			memcpy(to, src, size);
			return true;
		} else {
			// fill remaining buffer and advance source.
			const byte* from = src;
			size -= iter->remain;
			src += iter->remain;
			memcpy(iter->data, from, iter->remain);
			// get some more buffer space.
			if (!iter->more(iter))
				return false;
		}
	}
}

byte stream_read_byte(StreamIter* iter)
{
	if (iter->remain) {
		iter->remain--;
		return *iter->data++;
	}
	// slow path
	if (iter->more(iter)) {
		iter->remain--;
		return *iter->data++;
	}
	return 0;
}

void stream_write_byte(StreamIter* iter, byte value)
{
	if (iter->remain) {
		iter->remain--;
		*iter->data++ = value;
		return;
	}
	// slow path
	if (iter->more(iter)) {
		iter->remain--;
		*iter->data++ = value;
	}
}

int stream_read_int(StreamIter* iter)
{
	unsigned int bits = stream_read_uint(iter);
	int value;
	if (bits & 1) value = -(int)(bits >> 1); // negative.
	else value = (int)(bits >> 1); // non-negative.
	return value;
}

void stream_write_int(StreamIter* iter, int value)
{
	unsigned int bits;
	// move the sign bit to the least significant bit.
	// NB. only negatable int values can be encoded.
	if (value < 0) bits = (((unsigned int)-value) << 1) | 1;
	else bits = (unsigned int)value << 1;
	stream_write_int(iter, bits);
}

unsigned int stream_read_uint(StreamIter* iter)
{
	unsigned int bits = 0;
	unsigned int shift = 0;
	unsigned int val;
	do {
		// inline stream_read_byte with eof guard.
		if (iter->remain) {
			iter->remain--;
			val = *iter->data++;
		}
		else {
			if (iter->more(iter)) continue;
			else return 0; // eof.
		}
		// unpack the value bits.
		bits |= (val & 0x7F) << shift;
		shift += 7;
	} while (val & 0x80);
	return bits;
}

void stream_write_uint(StreamIter* iter, unsigned int bits)
{
	// pack the bits 7 to a byte.
	do {
		unsigned int rem = (bits >> 7);
		byte b;
		if (rem) b = (byte)(0x80 | (bits & 0x7f));
		else b = (byte)bits;
		bits = rem;
		// inline stream_write_byte with eof guard.
		if (iter->remain) {
			iter->remain--;
			*iter->data++ = b;
		}
		else if (iter->more(iter)) {
			iter->remain--;
			*iter->data++ = b;
		}
		else return; // no more space.
	}
	while (bits);
}
