#include "defs.h"
#include "ui.h"
#include "ui_win_impl.h"
#include "surface.h"
#include "blend.h"

struct UI_RenderToView {
	UI_Window* view;
	BITMAPINFO bmi;
	byte* bits;
};

static const int c_scanBufSize = 64 * 1024; // 64K scanline buffer.

UI_RenderToView* ui_create_render_to_view(UI_Window* w)
{
	UI_RenderToView* r = cpart_new(UI_RenderToView);
	BITMAPINFOHEADER h = { sizeof(BITMAPINFOHEADER), 0, 0, 1, 24, BI_RGB, 0, 0, 0, 0, 0 };
	r->bmi.bmiHeader = h;
	r->view = w;
	r->bits = malloc(c_scanBufSize);
	return r;
}

static COLORREF c_grey = RGB(127,127,127);

void ui_render_to_view(UI_RenderToView* r, UI_PaintEvent* e,
	struct SurfaceData* surf,
	irect doc, irect view, int scale, int anti)
{
	HDC dc = ((UI_PaintRequest*)e)->dc;
	int old, remain, lines, top, width, stride, ylerp;
	int dt, dl;
	byte *wr, *src;

	r->bmi.bmiHeader.biWidth = view.right - view.left;

	width = view.right - view.left;
	if (width <= 0)
		return; // view rect is zero width or invalid.

	stride = ((3 * width) + 3) & ~3;
	if (stride > c_scanBufSize)
		return; // this is bad; at least avoid infinite loop.

	old = SetStretchBltMode(dc, COLORONCOLOR);

	dl = (doc.left >> FP_BITS); dt = (doc.top >> FP_BITS);
	assert(dl >= 0 && dt >= 0);
	assert(dl + ((doc.right - doc.left)>>FP_BITS) <= surf->width);
	assert(dt + ((doc.bottom - doc.top)>>FP_BITS) <= surf->height);

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

		// copy buffered lines to screen.
		if (lines > 0)
		{
			r->bmi.bmiHeader.biHeight = lines;
			StretchDIBits(dc,
				view.left, top, view.right - view.left, lines,
				0, 0, view.right - view.left, lines,
				wr, &r->bmi, DIB_RGB_COLORS, SRCCOPY);
			top += lines;
		}
		else break; // cannot happen.
	}

	SetStretchBltMode(dc, old);
}
