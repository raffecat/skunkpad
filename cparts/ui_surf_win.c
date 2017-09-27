#include "defs.h"
#include "ui.h"
#include "ui_win_impl.h"
#include "render.h"
#include "surface.h"
#include "blend.h"

typedef void (*fill_func)(byte* dst, int len, RGBA colour);
typedef void (*blend_func)(byte* dst, int len, byte* src, int lerp, int inc, int alpha);

struct UI_RenderToView {
	Renderer sr;
	UI_Window* view;
	BITMAPINFO bmi;
	byte* bits;
	byte* last; // pointer to last line in bits.
	int top, left, width, height, stride;
};

static fill_func fill_opaque[] = {
	span3s_col_copy,	// blendModeCopy
	span3s_col_copy,	// blendModeSrcAlpha
};
static fill_func fill_alpha[] = {
	span3s_col_copy,	// blendModeCopy
	span3s_col_over,	// blendModeSrcAlpha
};
static blend_func blend_funcs[] = {
	span3s_4_copy_nn,	// blendModeCopy
	span3s_4p_over_nn,	// blendModeSrcAlpha
};
static blend_func blend_a_funcs[] = {
	span3s_4_copy_nn,	// blendModeCopy
	span3s_4p_over_nn_a,// blendModeSrcAlpha
};

static const int c_scanBufSize = 64 * 1024; // scanline buffer.

static void raw_fill_rect(UI_RenderToView* r, int top, int left,
	int width, int height, RGBA col, GfxBlendMode mode)
{
	// render spans; clipping has already been done.
	byte* to = r->last - (top * r->stride) + (left * 3);
	fill_func f;
	if (col.a == 255) f = fill_opaque[mode];
	else if (col.a) f = fill_alpha[mode];
	else return; // zero alpha.
	while (height--) {
		f(to, width, col);
		to -= r->stride; // upside-down.
	}
}

static void fill_quad(Graphics sr, float* verts, RGBA col, GfxBlendMode mode)
{
	UI_RenderToView* r = (UI_RenderToView*)sr;
	// convert to fixed point coords.
	int itop = (int)(top - r->top);
	int ibottom = (int)(bottom - r->top);
	int ileft = (int)(left - r->left);
	int iright = (int)(right - r->left);
	// clip rect to the destination surface.
	if (itop < 0) itop = 0;
	if (ibottom > r->height) ibottom = r->height;
	if (ileft < 0) ileft = 0;
	if (iright > r->width) iright = r->width;
	if (ileft >= iright || itop >= ibottom) return;
	// render spans.
	raw_fill_rect(r, itop, ileft, iright - ileft, ibottom - itop, col, mode);
}

static void raw_draw_surf(UI_RenderToView* r,
	int top, int left, int width, int height,
	SurfaceData* sd, int sx, int sy, int ix, int iy, BlendInfo blend)
{
	// render sampled spans; clipping has already been done.
	int alpha = (int)(blend.alpha * 255.0f);
	int to_stride, from_stride;
	byte *to, *from;
	blend_func f;
	if (alpha == 255) f = blend_funcs[blend.mode];
	else if (alpha) f = blend_a_funcs[blend.mode];
	else return; // zero alpha.
	to = r->last - (top * r->stride) + (left * 3);
	from = sd->data + ((sx >> FP_BITS) * 4);
	to_stride = r->stride;
	from_stride = sd->stride;
	sx = sx & (FP_ONE-1); // keep the sub-pixel part.
	while (height--) {
		f(to, width, from + ((sy >> FP_BITS) * from_stride), sx, ix, alpha);
		sy += iy;
		to -= to_stride; // upside-down.
	}
}

static void draw_image(Graphics sr, GfxImage img,
					   fRect from, fRect rect, BlendInfo blend)
{
	UI_RenderToView* r = (UI_RenderToView*)sr;
	SurfaceData* sd = (SurfaceData*)img;
	float x_scale, y_scale, x_anti, y_anti;
	int d_top, d_left, d_width, d_height;
	float e_right, e_bottom;

	// 2.0 if dest is twice as large as source.
	x_scale = (from.left < from.right) ?
		((rect.right - rect.left) / (from.right - from.left)) : 1.0f;
	y_scale = (from.top < from.bottom) ?
		((rect.bottom - rect.top) / (from.bottom - from.top)) : 1.0f;
	// 0.5 if dest is twice as large as source.
	x_anti = 1.0f / x_scale;
	y_anti = 1.0f / y_scale;

	// translate dest rect into surface space.
	rect.top -= r->top; rect.bottom -= r->top;
	rect.left -= r->left; rect.right -= r->left;

	// clip dest rect to surface, cut away source rect.
	if (rect.top < 0.0f) {
		from.top -= rect.top * y_anti;
		rect.top = 0.0f;
	}
	if (rect.left < 0.0f) {
		from.left -= rect.left * x_anti;
		rect.left = 0.0f;
	}
	if (rect.right > r->width) {
		from.right -= (r->width - rect.right) * x_anti;
		rect.right = (float)r->width;
	}
	if (rect.bottom > r->height) {
		from.bottom -= (r->height - rect.bottom) * x_anti;
		rect.bottom = (float)r->height;
	}

	// here we know that the dest rect is entirely within the dest
	// surface, or empty - top and left could still be large values.
	if (rect.left >= rect.right || rect.top >= rect.bottom)
		return; // empty dest rect.

	// convert dest rect to fixed point coords, now that we know
	// the float values are inside the dest surface.
	d_top = (int)(rect.top * 256.0f);
	d_left = (int)(rect.left * 256.0f);
	d_width = (int)(rect.right - rect.left);
	d_height = (int)(rect.bottom - rect.top);

	// remove rows above the source surface from the problem space.
	if (from.top < 0) {
		float clip; int rows;
		clip = -from.top * y_scale;
		if (clip > d_height) clip = (float)d_height;
		rows = (int)clip; // 0 < clip <= d_height
		// // raw_fill_rect(r, d_top>>8, d_left>>8, d_width, rows, bg);
		d_height -= rows;
		d_top += rows << 8;
		from.top = 0.0f; // NB. dropped remainder of -from.top
	}

	// remove columns left of the source surface from the problem space.
	if (from.left < 0) {
		float clip; int cols;
		clip = -from.left * x_scale;
		if (clip > d_width) clip = (float)d_width;
		cols = (int)clip; // 0 < clip <= d_width
		// // raw_fill_rect(r, d_top>>8, d_left>>8, cols, d_height, bg);
		d_width -= cols;
		d_left += cols << 8;
		from.left = 0.0f; // NB. dropped remainder of -from.left
	}

	// determine first dest pixel out of bounds in source surface.
	e_right = (from.left < sd->width) ?
		((sd->width - from.left) * x_scale) : 0.0f;
	e_bottom = (from.top < sd->height) ?
		((sd->height - from.top) * y_scale) : 0.0f;

	// remove columns that begin > e_right from the problem space.
	if (d_width > e_right) {
		int cols = 1 + (int)(d_width - e_right);
		if (cols > d_width) cols = d_width;
		// // raw_fill_rect(r, d_top>>8, (d_left>>8) + d_width - cols,
		// // 	cols, d_height, bg);
		d_width -= cols;
	}

	// remove rows that begin > e_bottom from the problem space.
	if (d_height > e_bottom) {
		int rows = 1 + (int)(d_height - e_bottom);
		if (rows > d_height) rows = d_height;
		// // raw_fill_rect(r, (d_top>>8) + d_height - rows, d_left>>8,
		// // 	d_width, rows, bg);
		d_height -= rows;
	}

	// all remaining dest pixels map to coords within the source.
	//raw_fill_rect(r, d_top>>8, d_left>>8, d_width, d_height, test);
	raw_draw_surf(r, d_top>>8, d_left>>8, d_width, d_height, sd,
		(int)(from.left * FP_ONE), (int)(from.top * FP_ONE),
		(int)(x_anti * FP_ONE), (int)(y_anti * FP_ONE), blend);
}

UI_RenderToView* ui_create_render_to_view(UI_Window* w)
{
	UI_RenderToView* r = cpart_new(UI_RenderToView);
	BITMAPINFOHEADER h = { sizeof(BITMAPINFOHEADER), 0, 0, 1, 24, BI_RGB, 0, 0, 0, 0, 0 };
	r->bmi.bmiHeader = h;
	r->view = w;
	r->bits = malloc(c_scanBufSize);
	r->sr.fillQuad = fill_quad;
	r->sr.drawImage = draw_image;
	return r;
}

void ui_render_to_view(UI_RenderToView* r, UI_PaintEvent* e,
	UI_RenderFunc draw, void* context)
{
	HDC dc = ((UI_PaintRequest*)e)->dc;
	int old, width, stride, top, lines;
	iRect rect = e->rect;

	width = e->rect.right - e->rect.left;
	if (width <= 0)
		return; // rect is zero width or invalid.

	// determine height of the rendering strip.
	stride = ((3 * width) + 3) & ~3;
	lines = c_scanBufSize / stride;
	if (lines < 1)
		return; // this is bad.

	r->bmi.bmiHeader.biWidth = width;
	r->bmi.bmiHeader.biHeight = lines;
	r->last = r->bits + (lines * stride) - stride;
	r->width = width;
	r->height = lines;
	r->stride = stride;
	r->left = e->rect.left;

	old = SetStretchBltMode(dc, COLORONCOLOR);

	// render batches of lines until done.
	top = e->rect.top;
	while (top < e->rect.bottom)
	{
		r->top = top;

		// reduce lines for the final strip.
		if (top + lines > e->rect.bottom)
		{
			lines = e->rect.bottom - top;
			r->bmi.bmiHeader.biHeight = lines;
			r->last = r->bits + (lines * stride) - stride;
			r->height = lines;
		}

		// ask caller to render the strip.
		rect.top = top;
		rect.bottom = top + lines;
		draw(context, r, rect);

		// copy buffered lines to screen.
		StretchDIBits(dc,
			e->rect.left, top, width, lines,
			0, 0, width, lines,
			r->bits, &r->bmi, DIB_RGB_COLORS, SRCCOPY);

		// advance to next strip of lines.
		top += lines;
	}

	SetStretchBltMode(dc, old);
}


/*
void render_something()
{
	ylerp = 0; // interpolator. TODO: initial value?

	// init for scaling down. TODO: top and left are already
	// less accurate than one pixel - it doesn't seem to matter
	// that we ignore the sub-pixel offset. still, those offsets
	// should feed into the interpolator initial values somehow.
	src = surf->data +
		((doc.top >> FP_BITS) * surf->stride) +
		(doc.left >> FP_BITS) * 4;
	lines = 0;

	// render batches of lines until done.
	top = view.top;
	while (top < view.bottom)
	{
		wr = r->bits + c_scanBufSize; // start at end of buffer.
		remain = c_scanBufSize;

		if (1 || scale <= FP_ONE) // scaling up?
		{
			lines = 0;

			// render lines using nearest-neighbour sampling.
			while (remain >= stride && top + lines < view.bottom)
			{
				wr -= stride; // write lines in reverse order.

				assert((doc.top >> FP_BITS) < surf->height);
				src = surf->data + ((doc.top >> FP_BITS) * surf->stride);

				// render a single line.
				// swap RGBA -> BGR for DIB format.
				span_copy_nn_rgba8_bgr8(src, wr, width, doc.left, scale);

				doc.top += scale;

				lines++;
				remain -= stride;
			}
		}
		else // scaling down.
		{
			// zero the first line.
			wr -= stride; // write lines in reverse order.
			memset(wr, 0, stride);

			// on second or subsequent batch, finish off the logic
			// below the break statement that would have run below
			// if we hadn't stopped to call StretchDIBits.
			if (lines)
			{
				// accumulate the area within the lower line.
				span_sum_adj_a_rgba8_bgr8(src, wr, width, 0, anti, ylerp);

				src += surf->stride; // next source line.
			}

			lines = 0;

			// accumulate lines into buffer.
			for (;;)
			{
				int ny = ylerp + anti;
				if (ny < FP_ONE)
				{
					// source line is entirely within one dest line.
					span_sum_adj_a_rgba8_bgr8(src, wr, width, 0, anti, anti);
					ylerp = ny;
				}
				else
				{
					// source line spans two dest lines.
					// accumulate the area within the upper line.
					span_sum_adj_a_rgba8_bgr8(src, wr, width, 0, anti, FP_ONE - ylerp);
					ylerp = ny - FP_ONE;

					// advance to the next dest line.
					lines++;
					remain -= stride;
					if (remain < stride || top + lines >= view.bottom)
						break; // buffer full or finished.

					// zero the next line.
					wr -= stride; // write lines in reverse order.
					memset(wr, 0, stride);

					// accumulate the area within the lower line.
					span_sum_adj_a_rgba8_bgr8(src, wr, width, 0, anti, ylerp);
				}

				src += surf->stride; // next source line.
			}
		}

	}
}
*/
