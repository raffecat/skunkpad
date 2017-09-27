#ifndef CPART_STREAM_ENC
#define CPART_STREAM_ENC


// Encoding for streams.

struct StreamIter;

bool stream_read(struct StreamIter* iter, byte* dest, size_t size);
bool stream_write(struct StreamIter* iter, const byte* src, size_t size);

byte stream_read_byte(struct StreamIter* iter);
void stream_write_byte(struct StreamIter* iter, byte value);

int stream_read_int(struct StreamIter* iter);
void stream_write_int(struct StreamIter* iter, int value);

unsigned int stream_read_uint(struct StreamIter* iter);
void stream_write_uint(struct StreamIter* iter, unsigned int value);


#endif
