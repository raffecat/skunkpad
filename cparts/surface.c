#include "defs.h"
#include "surface.h"
#include "blend.h"

#include <limits.h>


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
