#ifndef CPART_SURFACE
#define CPART_SURFACE

#ifndef CPART_BLEND
#include "blend.h"
#endif


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


#endif
