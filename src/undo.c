#include "defs.h"
#include "surface.h"
#include "pagebuf.h"
#include "skunkpad.h"

// Design:
// fixed size circular buffer.
// the insert "pos" is always immediately beyond the last record (init zero)
// each record begins with the (circular) distance from the previous record.
// a record encodes its own type and size.

// sentinel record: the last valid record is tagged "end" (overwritten
// when appending) so the "active" record is always valid.

// active record: "active" always holds the offset of the active record;
// undo never applies the active record; redo always applies it;
// append overwrites the active record and also 

// ready to append: the "active" record is the insertion point, but may
// be a valid record to "redo"; thus it is also the latest record undone.

// appending: [this is the fast path]
// determine pos for new record of size; set begin, copy to eobuf.
// set prev to new pos - last pos; set last pos to new pos.
// advance the insert pos to end of new record.
// let caller populate the new record.

// undo:
// if no records, stop.
// do {
// - apply its undo logic,
// - using its prev, rewind begin pos (wrap around)
// } while the record at begin pos is not undo-stop flagged,
// finally, request size of record at begin and update insert.

// redo:
// if no records, stop.
// do {
// - apply record redo logic, returning size.
// - using size, advance begin pos to next record (wrap around)
// } while the record at begin pos is not redo-stop flagged,
// finally, request size of record at begin and update insert.

struct UndoBuffer {
	byte* buf;      // undo records.
	size_t insert;  // insert pos; end of active record.
	size_t active;  // beginning of active record.
	size_t eobuf;   // beginning of last valid (redo) record.
	size_t bufsize; // total size of undo buffer.
};

typedef enum undo_tags {
	tag_end=0,
	tag_brush,
	tag_sample,
	tag_rect,
} undo_tags;

typedef struct record record;
struct record {
	uint16 prev;     // circular offset from beginning of prev record.
	byte tag, info;  // tag type of record, record-specific info.
};

static void* add_record(UndoBuffer* ub, size_t len)
{
}

static void writebuf(UndoBuffer* ub, const byte* buf, size_t len) {
	memcpy(ub->buf + ub->pos, buf, len);
	ub->pos += len;
}

// writenum costs 7 bits per 8 to store an int.
#define NUMSIZE ((sizeof(int)*8+6)/7)

static int writenum(byte* buf, int pos, int val)
{
	unsigned int bits;
	// move the sign bit to the least significant bit.
	// NB. only negatable int values can be encoded.
	if (val < 0) { bits = (((unsigned int)-val) << 1) | 1; }
	else { bits = (unsigned int)val << 1; }
	// pack the bits 7 to a byte.
	for (;;) {
		unsigned int rem = (bits >> 7);
		if (rem) {
			buf[pos++] = (byte)(0x80 | (bits & 0x7f));
			bits = rem;
		} else {
			buf[pos++] = (byte)bits;
			break;
		}
	}
	return pos;
}

static int readnum(byte* buf, int pos, int* result)
{
	unsigned int bits = 0;
	unsigned int val;
	int shift = 0;
	do {
		val = buf[pos++];
		bits |= (val & 0x7F) << shift;
		shift += 7;
	} while (val & 0x80);
	if (bits & 1) { *result = -(int)(bits >> 1); } // negative.
	else { *result = (int)(bits >> 1); } // non-negative.
	return pos;
}

static int writeint(byte* buf, int pos, int val) {
	int i; byte* p = (byte*)&val;
	for (i=0; i<sizeof(int); i++)
		buf[pos++] = *p++;
	return pos;
}

static int readint(byte* buf, int pos, int* result)
{
	int i; byte* p = (byte*)result;
	for (i=0; i<sizeof(int); i++)
		*p++ = buf[pos++];
	return pos;
}

static void save_rect(UndoBuffer* ub, SurfaceData* sd, int l, int t, int r, int b)
{
	// Save a rect of pixels from sd to the undo buffer.
	// NB. the rect {l,t,r,b} is known to be within bounds.
	int dx = l, dy = t;
	int pos, len, height = b-t, width = r-l;
	byte* src;
	byte buf[2+5*NUMSIZE];

	// Write rect position and size.
	buf[0] = tag_rect;
	pos = writenum(buf, 2, ub->size);
	pos = writenum(buf, pos, dx);
	pos = writenum(buf, pos, dy);
	pos = writenum(buf, pos, width);
	pos = writenum(buf, pos, height);
	assert(pos <= sizeof(buf) && pos <= 255+2);
	buf[1] = pos - 2;
	writebuf(&ub, buf, pos);

	len = width * 4;
	ub->size = pos + (len * height);

	// Copy the rect of pixels to the buffer.
	src = sd->data + (t * sd->stride) + (l * 4);
	while (height--) {
		pagebuf_write(&ub->iter, src, len);
		src += sd->stride;
	}
}

void begin_undo(UndoBuffer* ub, int layer, int size, int spacing, RGBA col)
{
	byte buf[2+4*NUMSIZE+4];
	int pos;
	buf[0] = tag_brush;
	// Write brush settings.
	pos = writenum(buf, 2, ub->size);
	pos = writenum(buf, pos, layer);
	pos = writenum(buf, pos, size);
	pos = writenum(buf, pos, spacing);
	buf[pos++] = (byte)col.r;
	buf[pos++] = (byte)col.g;
	buf[pos++] = (byte)col.b;
	buf[pos++] = (byte)col.a;
	assert(pos <= sizeof(buf) && pos <= 255+2);
	buf[1] = pos - 2;
	pagebuf_write(&ub->iter, buf, pos);
	ub->size = pos;
	// Reset "last" clipping rect.
	ub->last.left = 0; ub->last.top = 0;
	ub->last.right = 0; ub->last.bottom = 0;
}

void begin_undo_sample(UndoBuffer* ub, int qx, int qy, int alpha)
{
	byte buf[2+3*NUMSIZE+1];
	int pos;
	buf[0] = tag_sample;
	pos = writenum(buf, 2, ub->size);
	pos = writeint(buf, pos, qx);
	pos = writeint(buf, pos, qy);
	buf[pos++] = (byte)alpha;
	assert(pos <= sizeof(buf) && pos <= 255+2);
	buf[1] = pos - 2;
	pagebuf_write(&ub->iter, buf, pos);
	ub->size = pos;
}

void end_undo_sample(UndoBuffer* ub)
{
}

void save_undo_dab(UndoBuffer* ub, SurfaceData* sd, int left, int top, int width, int height)
{
	int right = left + width, bottom = top + height;
	// Clip the dab rect to the destination surface.
	if (left < 0) left = 0;
	if (top < 0) top = 0;
	if (right > sd->width) right = sd->width;
	if (bottom > sd->height) bottom = sd->height;
	if (left < right && top < bottom)
	{
		// Clip away the previous dab rect since we have already
		// saved the pixels underneath it. This certainly still allows
		// overlap during a stroke, but it's far better than saving the
		// full rect for every dab.
		if (top < ub->last.bottom && bottom > ub->last.top &&
			left < ub->last.right && right > ub->last.left)
		{
			// The rects overlap in both X and Y.
			int c_top = top, c_bottom = bottom;
			// First deal with parts above and below the prev rect.
			if (top < ub->last.top) {
				// There is a stripe above the prev rect.
				save_rect(ub, sd, left, top, right, ub->last.top);
				c_top = ub->last.top;
			}
			if (bottom > ub->last.bottom) {
				// There is a stripe below the prev rect.
				save_rect(ub, sd, left, ub->last.bottom, right, bottom);
				c_bottom = ub->last.bottom;
			}
			// Only the left and right stripes remain.
			// top and bottom have been clipped to the overlap area.
			if (left < ub->last.left) {
				save_rect(ub, sd, left, c_top, ub->last.left, c_bottom);
			}
			if (right > ub->last.right) {
				save_rect(ub, sd, ub->last.right, c_top, right, c_bottom);
			}
		}
		else
		{
			// No overlap - copy the entire new rect.
			save_rect(ub, sd, left, top, right, bottom);
		}
		// Save the new previous dab rect (clipped to sd)
		ub->last.left = left;
		ub->last.top = top;
		ub->last.right = right;
		ub->last.bottom = bottom;
	}
}

static void applyUndoRect(UndoBuffer* ub, byte* buf)
{
	int pos, dx, dy, width, height;
	pos = readnum(buf, 0, &dx);
	pos = readnum(buf, pos, &dy);
	pos = readnum(buf, pos, &width);
	pos = readnum(buf, pos, &height);
	// copy the rect of pixels.
	if (activeLayer) {
		/* -- make some sort of SpanSource iterator?
		SurfaceData* sd = &activeLayer->sd;
		// safety checks.
		if (width > 0 && height > 0 && ub->x + width <= sd->width && ub->y + height <= sd->height) {
			int len = width * 4;
			byte* dest = sd->data + (ub->y * sd->stride) + (ub->x * 4);
			while (height--) {
				pagebuf_read(&ub->iter, dest, len);
				dest += sd->stride;
			}
		}
		*/
	}
	// adjust current x,y by stored delta.
	ub->x -= dx; ub->y -= dy;
	assert(ub->x >= 0 && ub->y >= 0);
}

static void applyUndoBrush(byte* buf)
{
	int pos, layer, size, spacing;
	RGBA col;
	pos = readnum(buf, 0, &layer);
	pos = readnum(buf, pos, &size);
	pos = readnum(buf, pos, &spacing);
	col.r = buf[pos++];
	col.g = buf[pos++];
	col.b = buf[pos++];
	col.a = buf[pos++];
	// TODO: what if the layer was deleted?
	// undo deleting the layer? or invalidate undo for that layer?
	active_layer(layer);
	set_brush_size(size, size, spacing); // TODO: min/max
	set_brush_col(col);
}

static int undoEntry(UndoBuffer* ub)
{
	byte buf[256];
	int tag, pos, prevSize;
	size_t checkOne = pagebuf_tell(&ub->iter);
	size_t checkTwo, checkThree;
	// check if undo buffer contains any entries.
	if (ub->size == 0)
		return tag_none;
	// rewind to the beginning of the latest entry.
	pagebuf_rewind(&ub->iter, ub->size);
	checkTwo = pagebuf_tell(&ub->iter);
	// TODO: this fails on a page cross.
	assert(checkTwo + ub->size == checkOne);
	// read the entry header and previous entry size.
	pagebuf_read(&ub->iter, buf, 2);
	tag = buf[0];
	assert(buf[1] != 0);
	pagebuf_read(&ub->iter, buf, buf[1]);
	pos = readnum(buf, 0, &prevSize);
	assert(prevSize >= 0);
	// evaluate the entry.
	switch (tag)
	{
	case tag_rect: applyUndoRect(ub, buf + pos); break;
	case tag_sample: /* igore sample data */ break;
	case tag_brush: applyUndoBrush(buf + pos); break;
	default: assert(0); break;
	}
	checkThree = pagebuf_tell(&ub->iter);
	assert(checkThree == checkOne);
	// rewind to the beginning of the entry again.
	pagebuf_rewind(&ub->iter, ub->size);
	checkThree = pagebuf_tell(&ub->iter);
	assert(checkThree == checkTwo);
	ub->size = prevSize;
	return tag;
}

static int redoEntry(UndoBuffer* ub)
{
	// must assume ub->iter is at eof or a header.
}

void undo(UndoBuffer* ub)
{
	// apply entries until a brush entry is found.
	int tag;
	do { tag = undoEntry(ub); } while (tag == tag_rect || tag == tag_sample);
	// repaint the client.
	invalidate_all();
}

void redo(UndoBuffer* ub)
{
}

UndoBuffer* undobuf_create(size_t size)
{
	UndoBuffer* ub = cpart_new(UndoBuffer);
	ub->buf = cpart_alloc(size);
	return ub;
}
