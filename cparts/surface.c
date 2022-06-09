#include "defs.h"
#include "surface.h"
#include "blend.h"

#include <limits.h>

import * from blend


// Surface protocol

typedef enum SurfaceFormat {
	surface_invalid = 0,
	surface_a8      = 1,
	surface_la8     = 2,
	surface_rgb8    = 3,
	surface_rgba8   = 4,
	surface_a16     = 16 + 1,
	surface_la16    = 16 + 2,
	surface_rgb16   = 16 + 3,
	surface_rgba16  = 16 + 4,
} SurfaceFormat;

#define surfaceInitInvalid(sd) ((sd)->format = surface_invalid)
#define surfaceValid(sd) ((sd)->format)
#define surfaceChannels(format) ((format) & 15)
#define surfaceBytesPerChannel(format) (1 + ((format) >> 4))
#define surfaceBytesPerPixel(format) (surfaceChannels(format)*surfaceBytesPerChannel(format))
#define surfaceIs16bit(format) ((format) & 16)

typedef struct SurfaceData {
	int format;           // data format.
	int width, height;    // in pixels, >= 0.
	size_t stride;        // in bytes.
	byte* data;
} SurfaceData;


// Surface library
// Pixel-addressed RGBA surfaces (no sub-pixel)

typedef void (*surface_span_func)(byte* dst, int len, byte* src, int alpha);

void surface_src_over(SurfaceData* sd, int x, int y, SurfaceData* src, int alpha);

void surface_create(SurfaceData* sd, int format, int width, int height);
void surface_destroy(SurfaceData* sd);
void surface_fill(SurfaceData* sd, RGBA colour);
void surface_fill_rect(SurfaceData* sd, RGBA colour, int x, int y, int width, int height);
void surface_fill_rect_nc(SurfaceData* sd, RGBA colour, int x, int y, int width, int height);
void surface_copy(SurfaceData* sd, int x, int y, SurfaceData* src);
void surface_blend(SurfaceData* sd, int x, int y, SurfaceData* src, surface_span_func op, int alpha);
void surface_premultiply(SurfaceData* sd);


// SurfaceCol16.

typedef struct SurfaceCol16 {
	BlendSource r;
	RGBA16 col;
} SurfaceCol16;

void surface_read_col16(SurfaceCol16* state);


// SurfaceReadRGBA16.

typedef struct SurfaceReadRGBA16 {
	BlendSource r;
	RGBA16* iter; // current read pos.
	int alpha; // [0,255]
	byte* row; // current row in sd.
	const SurfaceData* sd;
} SurfaceReadRGBA16;

void surface_read_rgba16(SurfaceReadRGBA16* state, const SurfaceData* sd, int alpha);


// Blend API.

void surface_blend_source(SurfaceData* sd, int x, int y, BlendSource* src, BlendMode mode);


// a surface reader, a scaling reader, an affine reader,
// a format conversion reader.

// Targets:
// aligned buffers: RGBA, RGBX; 8 or 16 bit.
// RGBR GBRG BRGB RGBR | GBRG BRGB RGBR GBRG | BRGB RGBR GBRG BRGB (16 pixels)

// Operations always consume some source iterator and mutate a target buffer.
// Can the source iterator load several xmm registers for the op?

// MOVDQA : aligned 128-bit (16-byte) move; reg->reg, mem->reg, reg->mem.
// MOVDQU : unaligned 128-bit (16-byte) move; mem->reg.
// MOVNTDQ : non-cached aligned 128-bit store; reg->mem.
// PXOR,PAND,PANDN : logic ops.
// PUNPCK[L,H][BW,WD,DQ,QDQ] : interleave elements from reg,reg or reg,mem.
// PSHUF[D,LW,HW] : swizzle dwords or words.
// PACKUSWB : pack words to bytes with saturation.
// PSLL[W,D,DQ], PSRLDQ : logical shift
// PMAXUB,PMAXSW,PMINUB,PMINSW : max and min.
// PAVGB,PAVGW : average unsigned : (a+b+1) >> 1
// PADDUS[BW] : add unsigned saturate
// PMULHUW : multiply unsigned words, store high part.
// PMADDWD : multiply accumulate pairs of adjacent words.

// Intel paper: movdqu is suitable for fetching byte-aligned groups of 16 bytes
// from memory, but not useful for storing them.
// The Barcelona architecture prefers movaps for stores. movaps, movdqa, and
// movapd are functionally equivalent, with movaps having shorter encoding.

// __declspec(align(16)), _mm_malloc()
// pxor xmm7,xmm7 ; punpcklbw xmm0,xmm7 ; -- expand bytes to words.


/*
// Tile surface

typedef struct TilePool TilePool;
typedef struct TileSurface TileSurface;
typedef void (*TileSurfaceFunc)(SurfaceData* sd, int x, int y, void* context);

TilePool* tilepool_create();
void tilepool_destroy(TilePool* tp);

TileSurface* tilesurface_create(TilePool* tp, int width, int height);
void tilesurface_destroy(TileSurface* ts);
void tilesurface_clear(TileSurface* ts);
void tilesurface_read(TileSurface* ts, int x, int y, int width, int height,
					  TileSurfaceFunc func, void* context);
void tilesurface_write(TileSurface* ts, int x, int y, int width, int height,
					  TileSurfaceFunc func, void* context);
*/


void surface_create(SurfaceData* sd, int format, int width, int height)
{
	size_t stride, size, bytespp;
	assert(surfaceChannels(format) >= 1 && surfaceChannels(format) <= 4);
	assert(surfaceBytesPerChannel(format) >= 1 && surfaceBytesPerChannel(format) <= 2);
	sd->format = format;
	if (width < 0) width = 0; if (height < 0) height = 0;
	bytespp = surfaceBytesPerPixel(format);
	stride = width * bytespp; size = stride * height;
	sd->width = width; sd->height = height; sd->stride = stride;
	sd->data = malloc(size);
}

void surface_destroy(SurfaceData* sd)
{
	free(sd->data);
	sd->format = 0; sd->width = 0; sd->height = 0; sd->stride = 0; sd->data = 0;
}

void surface_fill_rect_nc(SurfaceData* sd, RGBA colour, int x, int y, int width, int height)
{
	byte* to;
	size_t adv;
	
	// no clip: x,y,width,height rect is within the surface.
	assert(x >= 0 && y >= 0 && width >= 0 && height >= 0);
	assert(x+width <= sd->width && y+height <= sd->height);

	to = sd->data + (y * sd->stride) + (x * surfaceBytesPerPixel(sd->format));
	adv = sd->stride;

	if (sd->format == surface_rgba8)
	{
		while (height--) {
			span4_col_copy(to, width, colour);
			to += adv;
		}
	}
	else if (sd->format == surface_rgba16)
	{
		//SurfaceCol16 src;
		//surface_read_col16(&src);
		//src.col = rgba8_to_rgba16(colour);
		RGBA16 col = rgba8_to_rgba16(colour);
		while (height--) {
			span8_col_copy((RGBA16*)to, width, col);
			to += adv;
		}
	}
}

void surface_fill(SurfaceData* sd, RGBA colour)
{
	surface_fill_rect_nc(sd, colour, 0, 0, sd->width, sd->height);
}

void surface_fill_rect(SurfaceData* sd, RGBA colour, int x, int y, int width, int height)
{
	// clip rect to surface.
	if (x < 0) { width += x; x = 0; }
	if (y < 0) { height += y; y = 0; }
	if (width > 0 && height > 0) {
		// x,y are >= 0, width,height are > 0.
		if (width > sd->width - x) width = sd->width - x;
		if (height > sd->height - y) height = sd->height - y;
		if (width > 0 && height > 0) {
			// x,y,width,height rect is within the surface.
			surface_fill_rect_nc(sd, colour, x, y, width, height);
		}
	}
}

void surface_copy(SurfaceData* sd, int x, int y, SurfaceData* src)
{
	surface_blend(sd, x, y, src, span4_4_copy, 0);
}

void surface_blend(SurfaceData* sd, int x, int y, SurfaceData* src, surface_span_func op, int alpha)
{
	// clip source rect to surface.
	int ox = 0, oy = 0;
	int width = src->width, height = src->height;
	assert(sd->format == surface_rgba8);
	if (x < 0) { ox -= x; width += x; x = 0; }
	if (y < 0) { oy -= y; height += y; y = 0; }
	if (width > 0 && height > 0) {
		// x,y are >= 0, width,height are > 0.
		if (width > sd->width - x) width = sd->width - x;
		if (height > sd->height - y) height = sd->height - y;
		if (width > 0 && height > 0) {
			// x,y,width,height rect is within the dest surface,
			// and ox,oy,width,height rect is within the source.
			byte* to = sd->data + (y * sd->stride) + (x * 4);
			byte* from = src->data + (oy * src->stride) + (ox * 4);
			size_t to_adv = sd->stride;
			size_t from_adv = src->stride;
			// clamp alpha to valid range.
			if (alpha<0) alpha=0; else if (alpha>255) alpha=255;
			// blend the spans.
			while (height--) {
				op(to, width, from, alpha);
				to += to_adv;
				from += from_adv;
			}
		}
	}
}

void surface_premultiply(SurfaceData* sd)
{
	int height = sd->height;
	byte* data = sd->data;
	if (sd->format == surface_rgba8)
	{
		while (height--) {
			int width = sd->width;
			byte* to = data;
			while (width--) {
				int alpha = 1 + to[3];
				to[0] = (to[0] * alpha) >> 8;
				to[1] = (to[1] * alpha) >> 8;
				to[2] = (to[2] * alpha) >> 8;
				to += 4;
			}
			data += sd->stride;
		}
	}
	else if (sd->format == surface_la8)
	{
		while (height--) {
			int width = sd->width;
			byte* to = data;
			while (width--) {
				int alpha = 1 + to[1];
				to[0] = (to[0] * alpha) >> 8;
				to += 2;
			}
			data += sd->stride;
		}
	}
}


// Blend sources.

static void surface_begin_noop(BlendSource* self, int x, int y, int width, int height) {}
static void surface_next_noop(BlendSource* self) {}


// SurfaceCol16

static void surface_read8_col16(BlendSource* self, BlendBuffer* data) {
}

static void surface_read1_col16(BlendSource* self, RGBA16* col) {
	SurfaceCol16* src = (SurfaceCol16*)self;
	*col = src->col;
}

void surface_read_col16(SurfaceCol16* src) {
	src->r.begin = surface_begin_noop;
	src->r.next = surface_next_noop;
	src->r.read8 = surface_read8_col16;
	src->r.read1 = surface_read1_col16;
	src->r.width = INT_MAX;
	src->r.height = INT_MAX;
}


// SurfaceReadRGBA16.

static void surface_begin_rgba16(BlendSource* self, int x, int y, int width, int height) {
	SurfaceReadRGBA16* state = (SurfaceReadRGBA16*)self;
	state->row = state->sd->data + y * state->sd->stride + x * 8;
	state->iter = (RGBA16*)state->row;
}

static void surface_next_rgba16(BlendSource* self) {
	SurfaceReadRGBA16* state = (SurfaceReadRGBA16*)self;
	state->row += state->sd->stride;
	state->iter = (RGBA16*)state->row;
}

static void surface_read8_rgba16(BlendSource* self, BlendBuffer* data) {
}

static void surface_read1_rgba16(BlendSource* self, RGBA16* col) {
	SurfaceReadRGBA16* state = (SurfaceReadRGBA16*)self;
	*col = *state->iter++;
}

void surface_read_rgba16(SurfaceReadRGBA16* state, const SurfaceData* sd, int alpha) {
	state->r.begin = surface_begin_rgba16;
	state->r.next = surface_next_rgba16;
	state->r.read8 = surface_read8_rgba16;
	state->r.read1 = surface_read1_rgba16;
	state->r.width = sd->width;
	state->r.height = sd->height;
	state->sd = sd;
	state->alpha = alpha;
}


// API.

void surface_blend_source(SurfaceData* sd, int x, int y, BlendSource* src, BlendMode mode)
{
	static const blend_rgba8_func b8funcs[] = {
		blend_rgba8_copy,
		blend_rgba8_normal,
		blend_rgba8_add,
		blend_rgba8_subtract,
	};
	// clip source rect to surface.
	int ox = 0, oy = 0;
	int width = src->width, height = src->height;
	if (x < 0) { ox -= x; width += x; x = 0; }
	if (y < 0) { oy -= y; height += y; y = 0; }
	if (width > 0 && height > 0) {
		// x,y are >= 0, width,height are > 0.
		if (width > sd->width - x) width = sd->width - x;
		if (height > sd->height - y) height = sd->height - y;
		if (width > 0 && height > 0) {
			// set up the source iterator.
			src->begin(src, ox, oy, width, height);
			// x,y,width,height rect is within the dest surface,
			// and ox,oy,width,height rect is within the source.
			if (sd->format == surface_rgba8) {
				byte* dst = sd->data + (y * sd->stride) + (x * 4);
				size_t stride = sd->stride;
				blend_rgba8_func blend = b8funcs[mode];
				if (!blend) return;
				// blend the spans.
				while (height--) {
					blend((RGBA*)dst, width, src);
					dst += stride;
					src->next(src);
				}
			}
			else if (sd->format == surface_rgba16) {
				// TODO.
			}
		}
	}
}
