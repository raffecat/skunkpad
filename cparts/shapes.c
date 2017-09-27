#include "defs.h"
#include "shapes.h"
#include "blend.h"
#include "surface.h"


// edge generators.

void shape_circle_fill(int radius, shape_span_func span, void* data)
{
	int f = 1 - radius;
	int x = radius;
	int y = 0;

	span(data, -radius, 0, radius+radius+1);

	while (x > y)
	{
		if (f >= 0)
		{
			if (x-1 != y)   // avoid overlap
			{
				span(data, -y, -x, y+y+1);
				span(data, -y,  x, y+y+1);
			}

			x--;
			f -= x+x;    // -2x + 2
		}

		y++;
		f += y+y+1;  // 1 + 2y (first value is 3)

		span(data, -x, -y, x+x+1);
		span(data, -x,  y, x+x+1);
	}
}


// span renderers.

typedef struct ShapeSpanFillRGBA ShapeSpanFillRGBA;
struct ShapeSpanFillRGBA {
	void (*fill)(byte* to, int len, RGBA colour);
	byte* data;
	int stride; // must be signed.
	RGBA col;
};

void shape_span_fill_rgba(void* data, int x, int y, int n) {
	ShapeSpanFillRGBA* args = data;
	byte* span = args->data + y * args->stride + x * 4;
	assert(sizeof(RGBA)==4);
	args->fill(span, n, args->col);
}

typedef struct ShapeSpanFillRGBA16 ShapeSpanFillRGBA16;
struct ShapeSpanFillRGBA16 {
	void (*fill)(RGBA16* dst, int len, RGBA16 colour);
	byte* data;
	int stride; // must be signed.
	RGBA16 col;
};

void shape_span_fill_rgba16(void* data, int x, int y, int n) {
	ShapeSpanFillRGBA16* args = data;
	byte* span = args->data + y * args->stride + x * 8;
	assert(sizeof(RGBA16)==8);
	args->fill((RGBA16*)span, n, args->col);
}


// drawing API.

void shape_circle_fill_rgba(SurfaceData* sd, int cx, int cy, int radius, RGBA colour)
{
	ShapeSpanFillRGBA args;
	assert(sd->format == surface_rgba8);
	// adjust sd so it is centered on the circle center.
	args.data = sd->data + cy * sd->stride + cx * 4;
	args.stride = (int)sd->stride;
	// choose span renderer based on colour alpha.
	if (colour.a < 255) {
		args.fill = span4_col_over;
		args.col = colour;
	} else {
		args.fill = span4_col_copy;
		args.col = colour;
	}
	// render the circle.
	shape_circle_fill(radius, shape_span_fill_rgba, &args);
}

void shape_circle_fill_rgba16(SurfaceData* sd, int cx, int cy, int radius, RGBA16 colour)
{
	ShapeSpanFillRGBA16 args;
	assert(sd->format == surface_rgba16);
	// adjust sd so it is centered on the circle center.
	args.data = sd->data + cy * sd->stride + cx * 8;
	args.stride = (int)sd->stride;
	// choose span renderer based on colour alpha.
	if (colour.a < 65535) {
		args.fill = span8_col_over;
		args.col = colour;
	} else {
		args.fill = span8_col_copy;
		args.col = colour;
	}
	// render the circle.
	shape_circle_fill(radius, shape_span_fill_rgba16, &args);
}

// Draw an anti-aliased line up to 1 pixel in width.
// Coordinates are signed Q23.8 with origin at the top left.
// Width must be 0 < Q8 <= 1.
// Colour must NOT be pre-multiplied by its alpha.

// Ideally we would generate these combinations:
// - unclipped, for higher level composites entirely within bounds.
// - clip to the surface - can be a sub-rect surface.
// - clip to an alpha mask surface and modulate alpha.
// - sample an affine mapped texture or gradient, apply alpha mask.
// - in general, apply N seperable shading steps.

// An alpha buffer for sampled coverage information.
// - theoretically infinite plane.
// - avoid sample merging during shape rendering to simplify things.
// - reserve worst-case buffer space in advance.
// - optional clipping rect to reduce unneeded rendering.
// - avoid merging when shapes cannot overlap (e.g. aabb)
// - perform span coalescing as a post-processing step.

// An alternative technique:
// - store lerp values for the clipped line segment,
// - fill a linear buffer with shape alpha values,
// - fill a linear buffer with samples from an alpha mask,
// - fill a linear buffer with affine texture samples,
// - fill a linear buffer with gradient samples,
// - combine linear buffers using any span blender or shader^,
// - render a linear buffer to any destination surface format.
// ^ NB. shaders must assume that samples are non-adjacent.
// In general this technique requires:
// + one sampler per shape per source, e.g. RGB tex2D, ARGB tex2D,
//   affine ARGB tex2D, linear gradient, radial gradient, etc.
// + fast SSE buffer combiners and buffer shaders.
// + one renderer per shape per destination, e.g. ARGB, BGR.
// Actually the affine cases could be factored:
// + one u,v generator per shape,
// + one affine sampler per source.
// This works well for relatively thin lines and curves.
// What about shapes with large areas of constant alpha?
// Generate span-length alpha unless required for shading.
// + also write span-length solid colour renderer per destination.
// + and a span-length linear buffer in-place modulator.
// What about shapes that generate a lot of pixels?
// - process one scanline at a time, or
// - suspend shape generation after filling a buffer (complexity!)

void shape_hairline(SurfaceData* sd, int x0, int y0, int x1, int y1, int width, RGBA colour)
{
	// modulate alpha by the width of the line.
	// thinner lines are rendered by reducing the alpha value.
	//int alpha = Q8_MUL(width, col.a);

}
