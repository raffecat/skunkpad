#include "defs.h"
#include "surface.h"
#include "shapes.h"
#include "tablet_input.h"  // for Tablet_InputEvent protocol.
#include "skunkpad.h"
#include "graphics.h"
#include "shapes.h"

#include <math.h>
#include <limits.h>


const Q8_BITS = 8
const Q8_ONE = 1 << Q8_BITS
const Q8_HALF = Q8_ONE >> 1
const Q8_MASK = Q8_ONE - 1

#define Q8_FLOOR(X) ((X) & ~Q8_MASK)
#define Q8_CEIL(X) (((X) | Q8_MASK) + 1)
#define Q8_IFLOOR(X) ((X) >> Q8_BITS)
#define Q8_ICEIL(X) (((X) + Q8_MASK) >> Q8_BITS)
#define Q8_TOF(X) ((X) * (1/256.0f))
#define Q8_FTOQ(X) ((int)((X) * Q8_ONE))

typedef uint16 sample; // colour sample.

struct DabPainter {
	GfxDraw draw;
	SurfaceData accum;
	SurfaceData* brush;
	float brushWidth, brushHeight;
	DabPainterOutput output;
	void* output_data;
	iRect dirty;		// Q8 in accum space.
	int remain;			// remaining dabs.
	int accum_left;		// Q8 in doc space.
	int accum_top;		// Q8 in doc space.
	int accum_size;		// width & height of GfxTarget.
	int size_min;		// Q8
	int size_range;		// Q8
	int spacing;		// Q8
	int alpha_min;		// 0-255
	int alpha_range;	// 0-255
	int prev_x, prev_y;	// Q8 document coords
	RGBA col;
	GfxBlendMode mode;
	bool started;
};

//GfxImage painter_get_accum(DabPainter* dp) { return dp->accum; }
//RGBA painter_get_col(DabPainter* dp) { return dp->col; }

const int c_accumSize = 128;
const RGBA c_transparent = {0};

export DabPainter* createDabPainter(GfxDraw draw)
{
	DabPainter* dp = cpart_new(DabPainter);
	dp->draw = draw;
	surfaceInitInvalid(&dp->accum);
	dp->accum_size = 0;
	dp->brush = 0;
	dp->size_min = Q8_ONE;		// one pixel.
	dp->size_range = 0;			// no scaling.
	dp->spacing = Q8_ONE / 10;	// ~10% of size.
	dp->alpha_min = 0;			// no bias.
	dp->alpha_range = 255;		// full range.
	dp->mode = gfxBlendPremultiplied;  // brush mode.
	dp->started = false;
	dp->dirty.left = dp->dirty.top = INT_MAX;
	dp->dirty.right = dp->dirty.bottom = 0;
	return dp;
}

void painter_upsize(DabPainter* dp)
{
	if (dp->draw) {
		// the accumulation target must be at least 1.5 times the
		// max size of the brush, so we can accumulate for at least
		// a distance of 1/4 the max brush size.
		int required = dp->size_min + dp->size_range;
		required += required >> 1; // div 2 >= 0
		// convert from Q8 to int pixels, rounding up.
		required = Q8_ICEIL(required);
		if (required < c_accumSize) required = c_accumSize;
		if (!surfaceValid(&dp->accum)) {
			surface_create(&dp->accum, surface_rgba16, required, required);
			surface_fill(&dp->accum, c_transparent);
			dp->accum_size = required;
		} else {
			// grow the target if necessary.
			if (required > dp->accum_size) {
				surface_destroy(&dp->accum);
				surface_create(&dp->accum, surface_rgba16, required, required);
				surface_fill(&dp->accum, c_transparent);
				dp->accum_size = required;
			}
		}
	}
}

export void painter_set_output(DabPainter* dp, DabPainterOutput func, void* data) {
	dp->output = func;
	dp->output_data = data;
}
export void painter_set_brush(DabPainter* dp, SurfaceData* brush) {
	assert(brush->format == surface_a8);
	if (brush->format == surface_a8) {
		dp->brush = brush;
		dp->brushWidth = (float)brush->width;
		dp->brushHeight = (float)brush->height;
	}
}
export void painter_set_colour(DabPainter* dp, RGBA col) {
	dp->col = col;
}
export void painter_set_blendMode(DabPainter* dp, GfxBlendMode mode) {
	dp->mode = mode;
}
export void painter_set_alpha_range(DabPainter* dp, int min, int max) {
	if (min<0) min=0; else if (min>255) min=255;
	if (max<min) max=min; else if (max>255) max=255;
	dp->alpha_min = min;             // [0,255]
	dp->alpha_range = max-min;       // ?? [256,1] since 0 <= pressure < 1
}
export void painter_set_size_range(DabPainter* dp, int min, int max) {
	if (min<0) min=0;
	if (max<min) max=min;
	dp->size_min = min;
	dp->size_range = max-min;
	// grow the accumulation target if necessary.
	painter_upsize(dp);
}
export void painter_set_spacing(DabPainter* dp, int ratio) {
	if (ratio<1) ratio = 1;
	dp->spacing = ratio;
}

void painter_flush(DabPainter* dp)
{
	// round coords OUT from Q8 to integer pixels.
	iRect dirty;
	dirty.left = Q8_IFLOOR(dp->dirty.left);
	dirty.top = Q8_IFLOOR(dp->dirty.top);
	dirty.right = Q8_ICEIL(dp->dirty.right);
	dirty.bottom = Q8_ICEIL(dp->dirty.bottom);

	// check if the dirty rect is non-empty.
	if (dirty.left < dirty.right && dirty.top < dirty.bottom)
	{
		// map the dirty rect to document space.
		// NB. rect may extend outside the document bounds.
		//dirty.left += Q8_IFLOOR(dp->accum_left);
		//dirty.top += Q8_IFLOOR(dp->accum_top);
		//dirty.right += Q8_IFLOOR(dp->accum_left);
		//dirty.bottom += Q8_IFLOOR(dp->accum_top);

		// destination rect is the entire accum area.
		dirty.left = Q8_IFLOOR(dp->accum_left);
		dirty.top = Q8_IFLOOR(dp->accum_top);
		dirty.right = dirty.left + dp->accum_size;
		dirty.bottom = dirty.top + dp->accum_size;

		if (dp->output)
		{
			iPair org;

			// determine the source top-left within the accum image.
			//org.x = Q8_IFLOOR(dp->dirty.left);
			//org.y = Q8_IFLOOR(dp->dirty.top);

			// source top-left is always the top-left of the accum image.
			org.x = org.y = 0;
			
			// run the callback to merge all deferred paint.
			dp->output(dp->output_data, &dp->accum, org, dirty);
		}

		// make the dirty rect empty.
		dp->dirty.left = dp->dirty.top = INT_MAX;
		dp->dirty.right = dp->dirty.bottom = 0;
	}
}

const int c_deferMinPixels = 10;

void painter_set_bounds(DabPainter* dp, int x, int y)
{
	int brushSize, maxDist, dist, remain;
	int half = dp->accum_size << (Q8_BITS-1); // to Q8, div 2 >= 0

	// center the Q8 accum bounds on the (x,y) coords,
	// but aligned on pixel boundaries (half way between samples)
	dp->accum_left = Q8_CEIL(x - half);
	dp->accum_top = Q8_CEIL(y - half);

	// calculate max number of dabs that can be painted before
	// deferred output must be flushed.
	brushSize = Q8_ICEIL(dp->size_min + dp->size_range);
	maxDist = (dp->accum_size >> 1) - ((brushSize + 1) >> 1);

	dist = brushSize;
	if (dist < c_deferMinPixels) dist = c_deferMinPixels;
	if (dist > maxDist) dist = maxDist;
	remain = (dist << Q8_BITS) / dp->spacing;
	dp->remain = remain;
}

void paint_start_painting(DabPainter* dp, bool clear)
{
	// clear the accum buffer if required.
	// start with a fully transparent layer.
	if (clear && surfaceValid(&dp->accum))
		surface_fill(&dp->accum, c_transparent);
}

void paint_end_painting(DabPainter* dp)
{
	// nothing to do.
}

void paint_dab_overflow(DabPainter* dp, int x, int y)
{
	// must stop painting so we can render the accum target.
	paint_end_painting(dp);

	// flush deferred paint to the document.
	painter_flush(dp);

	// begin painting to the accum target again.
	paint_start_painting(dp, true);

	// reset accum bounds centered on this dab.
	painter_set_bounds(dp, x, y);
}

void paint_dab(DabPainter* dp, int x, int y, int alpha, int size)
{
	int left, top;
	int limit = dp->accum_size << Q8_BITS; // to Q8.

	// check if the dab extends outside accum bounds.
	//if (left < 0 || top < 0 ||
	//	left + size > limit || top + size > limit)
	if (!--dp->remain)
	{
		// flush deferred paint to output and reset the painter.
		// NB. moved out of line because this is the slow path.
		paint_dab_overflow(dp, x, y);

		// translate top-left of dab to repositioned accum space.
		//left = x - (size>>1) - dp->accum_left;
		//top = y - (size>>1) - dp->accum_top;
	}

	// translate top-left of dab to accum space.
	left = x - (size>>1) - dp->accum_left,
	top = y - (size>>1) - dp->accum_top;

	// at this point we assume the dab fits within the accum
	// bounds, which will always be true unless painter_upsize
	// has failed to grow the accum target.

	// paint the dab into the accum target.
	if (dp->brush)
	{
		RGBA16 col;
		int px = Q8_IFLOOR(left), py = Q8_IFLOOR(top);
		int sz = Q8_IFLOOR(size);

		// pre-multiply RGB in [0,255] by alpha in [0,255].
		col.r = dp->col.r * (1+alpha);
		col.g = dp->col.g * (1+alpha);
		col.b = dp->col.b * (1+alpha);
		col.a = 255 * (1+alpha); // [0,65280]; >>8 -> [0,255]

		// accumulate alpha-only brush shape.
		// TODO: size.
		// surface_blend(&dp->accum, px, py, dp->brush, dp->mode, alpha);
		shape_circle_fill_rgba16(&dp->accum, px, py, sz, col);
	}

	// update the dirty rectangle with the dab extent.
	if (left < dp->dirty.left) dp->dirty.left = left;
	if (top < dp->dirty.top) dp->dirty.top = top;
	if (left+size > dp->dirty.right) dp->dirty.right = left+size;
	if (top+size > dp->dirty.bottom) dp->dirty.bottom = top+size;
}

export void painter_begin(DabPainter* dp, struct Tablet_InputEvent* e)
{
	int qx, qy;

	// map pressure to alpha and size.
	int alpha = dp->alpha_min + (int)(e->pressure * dp->alpha_range);
	int size = dp->size_min + (int)(e->pressure * dp->size_range);

	// quantize document coordinates to Q23.8 fixed point.
	qx = Q8_FTOQ(e->x);
	qy = Q8_FTOQ(e->y);

	dp->prev_x = qx;
	dp->prev_y = qy;

	// record the sample data for redo.
	// begin_undo_sample(qx, qy, alpha);

	// paint the first dab at the initial position.
	if (surfaceValid(&dp->accum))
	{
		// begin drawing to the accum target.
		paint_start_painting(dp, true);

		// reset accum bounds centered on this dab.
		painter_set_bounds(dp, qx, qy);

		// render the first dab to the accumulation target.
		// TODO: no need to check bounds, cannot cause a flush,
		// pos is center of accum, new dirty rect is same.
		// consider inlining the first dab draw so that the
		// paint_dab fast path can be moved into painter_draw.
		paint_dab(dp, qx, qy, alpha, size);
	}
}

export void painter_end(DabPainter* dp)
{
	// stop painting so the app can handle redraw.
	paint_end_painting(dp);

	if (surfaceValid(&dp->accum))
	{
		// commit any remaining deferred paint.
		painter_flush(dp);
	}
}

export void painter_begin_batch(DabPainter* dp)
{
	// begin drawing to the accum target.
	paint_start_painting(dp, false);
}

export void painter_end_batch(DabPainter* dp)
{
	// stop painting so the app can handle redraw.
	paint_end_painting(dp);

	// TODO FIXME TEST DEBUG display the accum buffer.
	invalidate_all();
}

export void painter_draw(DabPainter* dp, Tablet_InputEvent* e)
{
	int qx, qy;
	int x, y, dx, dy;
	double dist;

	// map pressure to alpha.
	int alpha = dp->alpha_min + (int)(e->pressure * dp->alpha_range);
	int size = dp->size_min + (int)(e->pressure * dp->size_range);

	//trace("pressure %f", e->pressure);

	if (surfaceValid(&dp->accum))
	{
		// quantize document coordinates to Q23.8 fixed point.
		qx = Q8_FTOQ(e->x);
		qy = Q8_FTOQ(e->y);

		// record the sample data for redo.
		// begin_undo_sample(qx, qy, alpha);

		// determine distance travelled since previous dab.
		x = dp->prev_x;
		y = dp->prev_y;
		dx = qx - x;
		dy = qy - y;
		dist = sqrt((double)dx*dx + (double)dy*dy);
		if (dist >= dp->spacing)
		{
			// dx,dy in Q8 * spacing in Q8 / dist in Q8.
			// normalize [dx,dy] vector, multiply by spacing.
			double inc_x = (double)dx * dp->spacing / dist;
			double inc_y = (double)dy * dp->spacing / dist;

			while (dist >= dp->spacing)
			{
				dist -= dp->spacing;
				x = (int)(x + inc_x);
				y = (int)(y + inc_y);

				// TODO: interpolate alpha and size along the line.

				paint_dab(dp, x, y, alpha, size);
			}

			// update previous dab position for next event.
			dp->prev_x = x;
			dp->prev_y = y;
		}
	}
}
