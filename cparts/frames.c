import draw, affine, surface;

// Frames - hierarchical visual frames.

// Each frame encompasses a stateful model and one view, insulated from any
// changes to the underlying display target.

// UI controls, layout, event bindings - this is in Lua right now.
// Composited box model, native controls - these Frames.

struct FrameRenderRequest {
	GfxDraw draw;
	fRect* clip;
	float alpha;
};

struct FrameHitTest {
	Frame frame;
	fPair pos;
};

struct FrameBlendImage {
	//GfxDraw draw;       // render target used to blend images.
	BlendMode mode;		// blend mode.
	int alpha;		    // blend alpha [0,255]
	SurfaceData* image;	// the source image to blend.
	iRect source;		// area of sorce image in pixels.
	iRect dest;			// area of the destination frame in pixels.
};

export enum FrameMessage {
	frameRender,	// FrameRenderRequest*
	frameHitTest,	// FrameHitTest*
	frameSetRect,	// fRect*
	frameSetColour, // RGBA*
	frameSetImage,  // GfxImage
	frameSetVisible,// bool*
	frameGetVisible,// bool*
	frameSetAlpha,  // float*
	frameGetAlpha,  // float*
	frameSetSize,   // iPair*
	frameGetSize,   // iPair*
	frameLoadSurfaceData, // SurfaceData*
	frameParentChanged, // 0
	frameGetUIWindow, // &(UI_Window*) . bool
	frameReleaseResources, // 0
	frameSetAffine, // float*
	frameGetAffine, // float*
	frameSetRenderContext, // RenderContext*
	frameBlendImage, // FrameBlendImage*
} FrameMessage;

let FrameMessageFunc = type (Frame, FrameMessage, ref any) -> int;

struct Frame {
	FrameMessageFunc message;
	Frame parent;
	Frame children;
	Frame next;
};

export const RGBA c_transparent = { 0, 0, 0, 0 };


// frame helpers.

Frame frame_alloc(size_t size, FrameMessageFunc func)
{
	Frame frame = cpart_alloc(size);
	frame.message = func;
	frame.parent = frame.children = frame.next = 0;
	return frame;
}

void frame_remove_from_parent_no_notify(Frame frame)
{
	Frame parent = frame.parent;
	if (parent && parent.children) {
		if (parent.children == frame) {
			parent.children = frame.next;
		} else {
			Frame prev = parent.children; // cannot be "frame"
			while (prev.next && prev.next != frame) prev = prev.next;
			// prev cannot be frame, next is frame or 0.
			if (prev.next == frame)
				prev.next = frame.next; // unlink frame.
		}
	}
	frame.parent = 0;
	frame.next = 0;
}

void frame_send_to_children(Frame frame, FrameMessage msg, void* data)
{
	Frame walk = frame.children;
	while (walk) {
		walk.message(walk, msg, data);
		walk = walk.next;
	}
}

int frame_send_to_ancestors(Frame frame, FrameMessage msg, void* data)
{
	Frame walk = frame.parent;
	while (walk) {
		if (walk.message(walk, msg, data))
			return 1; // ancestor handled the message.
		walk = walk.parent;
	}
	return 0; // did not find a willing ancestor.
}


// direct api.

export void frame_remove(Frame frame)
{
	if (frame.parent) {
		frame_remove_from_parent_no_notify(frame);
		// notify the removed frame that its parent has changed.
		frame.message(frame, frameParentChanged, 0);
	}
}

export void frame_insert(Frame frame, Frame parent, int after)
{
	Frame old_parent = frame.parent;
	if (old_parent)
		frame_remove_from_parent_no_notify(frame);
	if (!parent) parent = old_parent; // re-insert into same parent.
	if (parent) {
		frame.parent = parent;
		if (after == 0) {
			// insert as first child.
			frame.next = parent.children;
			parent.children = frame;
		}
		else {
			// insert after N child frames, append if N<0.
			if (parent.children) {
				Frame prev = parent.children;
				if (after < 0) after = INT_MAX;
				while (prev.next && --after) prev = prev.next;
				// insert into linked list.
				frame.next = prev.next;
				prev.next = frame;
			} else parent.children = frame;
		}
	}
	// notify the inserted frame if its parent has changed.
	if (parent != old_parent)
		frame.message(frame, frameParentChanged, 0);
}

export void frame_destroy(Frame frame)
{
	// recursively destroy children first.
	while (frame.children) {
		frame_destroy(frame.children);
	}
	// now destroy this frame.
	frame.message(frame, frameReleaseResources, 0);
	frame_remove(frame);
	cpart_free(frame);
}

export Frame frame_child(Frame frame, int index)
{
	Frame walk = frame.children;
	while (walk && index) { walk = walk.next; --index; }
	return walk;
}


// box model.

struct BoxFrame {
	Frame frame;
	fRect rect;
	GfxImage image;
	RGBA col;
	float alpha;
	bool show;
};

void box_draw(BoxFrame f, FrameRenderRequest* r)
{
	FrameRenderRequest req;
	req.draw = r.draw;
	req.clip = r.clip;
	req.alpha = r.alpha * f.alpha;

	if (f.show && req.alpha > 0)
	{
		// push state to save transform.
		GfxDraw_save(req.draw);

		// apply box translation.
		GfxDraw_translate(req.draw, f.rect.left, f.rect.top);

		if (f.image) {
			// render blended image.
			GfxDraw_blendMode(req.draw, gfxBlendNormal, req.alpha);
			GfxDraw_drawImageRect(req.draw, f.image, 0, 0,
				f.rect.right - f.rect.left, f.rect.bottom - f.rect.top);
		}
		else if (f.col.a) {
			// render blended rect.
			GfxDraw_blendMode(req.draw, gfxBlendNormal, req.alpha);
			GfxDraw_fillColor(req.draw, f.col.r/255.0f, f.col.g/255.0f,
										f.col.b/255.0f, f.col.a/255.0f);
			GfxDraw_fillRect(req.draw, 0, 0, f.rect.right - f.rect.left,
											 f.rect.bottom - f.rect.top);
		}

		// render all children transformed by (rect.left, rect.top)
		if (f.frame.children) {
			frame_send_to_children(&f.frame, frameRender, &req);
		}

		// pop state to restore transform.
		GfxDraw_restore(req.draw);
	}
}

/*bool box_clip_test(BoxFrame frame, fRect* bounds)
{
	return bounds.left <= frame.rect.right && bounds.top <= frame.rect.bottom &&
		   bounds.right >= frame.rect.left && bounds.bottom >= frame.rect.top;
}*/

int box_message(Frame frame, FrameMessage msg, void* data)
{
	// a message handler is the same as an interface with optional methods;
	// an interface could be compiled down to a message handler (or function table)
	BoxFrame box = (BoxFrame)frame;
	switch (msg)
	{
	case frameRender:
		box_draw(box, data);
		break;
	case frameSetRect:
		box.rect = *(fRect*)data;
		break;
	case frameSetColour:
		box.col = *(RGBA*)data;
		break;
	case frameSetImage: {
		GfxImage img = data;
		if (img) retain(img); // must retain first.
		if (box.image) release(box.image);
		box.image = img; }
		break;
	case frameSetVisible:
		box.show = *(bool*)data;
		break;
	case frameGetVisible:
		*(bool*)data = box.show;
		break;
	case frameSetAlpha:
		box.alpha = *(float*)data;
		break;
	case frameGetAlpha:
		*(float*)data = box.alpha;
		break;
	case frameReleaseResources:
		if (box.image)
			release(box.image);
		box.image = 0;
		break;
	}
	return 0;
}

export Frame frame_create_box(Frame parent)
{
	BoxFrame frame = frame_alloc(sizeof(BoxFrame), box_message);
	frame.rect.left = frame.rect.top = frame.rect.right = frame.rect.bottom = 0;
	frame.image = 0;
	frame.col = c_transparent;
	frame.alpha = 1;
	frame.show = true;
	frame_insert((Frame)frame, parent, -1); // append.
	return (Frame)frame;
}


// layer frame.

struct LayerFrame {
	Frame frame;
	GfxImage* grid; // array of GfxImage.
	GfxContext rc; // TODO: link to FrameContext.
	GfxBlendMode mode;
	float alpha;
	float x, y;
	int width, height;
	int tilesX, tilesY;
	bool show;
};

const int c_tileSize = 256;

bool any_visible(Frame f)
{
	// is this frame or any subsequent sibling visible?
	while (f) {
		bool vis = false;
		f.message(f, frameGetVisible, &vis);
		if (vis) return true;
		f = f.next;
	}
	return false;
}

__inline float min04(float a, float b, float c)
{
	float v = 0;
	if (a < 0) v = a;
	if (b < v) v = b;
	if (c < v) v = c;
	return v;
}

__inline float max04(float a, float b, float c)
{
	float v = 0;
	if (a > 0) v = a;
	if (b > v) v = b;
	if (c > v) v = c;
	return v;
}

// TODO: check if the tile is in the viewport; this is very worthwhile
// to avoid selecting (and therefore swapping in) the textures
// for tiles that will be clipped to the viewport anyway.

void lf_draw_tiles(LayerFrame f, FrameRenderRequest* r)
{
	GfxDraw draw = r.draw;
	GfxImage* grid = f.grid;
	int tilesX = f.tilesX, tilesY = f.tilesY;
	float x = 0.0f, y= 0.0f;
	float size = (float)c_tileSize;
	int ix, iy;
	assert(tilesX > 0 && tilesX < 1000); // TEST: corruption finding.
	assert(tilesY > 0 && tilesY < 1000);
	// render the grid of tiles.
	for (iy=0; iy<tilesY; ++iy) {
		for (ix=0; ix<tilesX; ++ix) {
			GfxImage tile = grid[iy*tilesX+ix];
			if (tile) {
				// render the tile.
				GfxDraw_drawImageRect(draw, tile, x, y, size, size);
			}
			x += size; // advance one tile right.
		}
		y += size; // advance one tile down.
		x = 0.0f; // return to left edge.
	}
}

void lf_draw(LayerFrame f, FrameRenderRequest* r)
{
	// blend to intermediate target if layer has children.
	if (f.frame.children && any_visible(f.frame.children))
	{
		FrameRenderRequest req;
		req.draw = r.draw;
		req.clip = r.clip;
		req.alpha = 1.0f;

		// create a new rendering layer.
		GfxDraw_pushLayer(req.draw);

		// render this layer without blending.
		GfxDraw_blendMode(req.draw, gfxBlendCopy, 1);
		lf_draw_tiles(f, r);

		// render any visible children at 100% alpha.
		frame_send_to_children(&f.frame, frameRender, &req);

		// blend the active layer to the previous.
		GfxDraw_popLayer(req.draw, f.mode, f.alpha);
	}
	else
	{
		// blend the layer content.
		GfxDraw_blendMode(r.draw, f.mode, f.alpha);
		lf_draw_tiles(f, r);
	}
}

void lf_discard(LayerFrame f)
{
	if (f.grid) {
		int oldX = (f.width + (c_tileSize-1)) / c_tileSize;
		int oldY = (f.height + (c_tileSize-1)) / c_tileSize;
		int i, num = oldX * oldY;
		for (i=0; i<num; i++) {
			if (f.grid[i])
				release(f.grid[i]);
		}
		cpart_free(f.grid);
		f.grid = 0;
	}
}

int ceilPowerOfTwo(int value) {
	// determine the smallest power of two >= value.
	// returns zero when value <= 0.
	if (value > 0) {
		int power = 0;
		--value;
		while (value) { value >>= 1; ++power; }
		return 1 << power;
	}
	return 0;
}

GfxImage lf_createTile(LayerFrame f, int width, int height)
{
	GfxImage img = GfxContext_createImage(f.rc);
	RGBA transparent = { 0, 0, 0, 0 };
	GfxImage_create(img, gfxFormatRGBA8, width, height, transparent,
		gfxImageMagNearest | gfxImageRenderTarget);
	return img;
}

void lf_resize(LayerFrame f, int width, int height)
{
	int tilesX, tilesY, ix, iy;
	lf_discard(f); // destroy old grid of tiles.
	f.width = width;
	f.height = height;
	tilesX = (width + (c_tileSize-1)) / c_tileSize;
	tilesY = (height + (c_tileSize-1)) / c_tileSize;
	f.tilesX = tilesX;
	f.tilesY = tilesY;
	if (tilesX && tilesY) {
		int wholeX, wholeY, partWidth, partHeight;
		f.grid = cpart_alloc(tilesX * tilesY * sizeof(GfxImage));
		if (!f.grid) return;
		// create full tiles for wholly covered cells.
		wholeX = (tilesX * c_tileSize == width) ? tilesX : (tilesX-1);
		wholeY = (tilesY * c_tileSize == height) ? tilesY : (tilesY-1);
		for (iy=0; iy<wholeY; iy++) {
			for (ix=0; ix<wholeX; ix++) {
				f.grid[iy * tilesX + ix] = lf_createTile(f, c_tileSize, c_tileSize);
			}
		}
		// determine nearest powers of two for partial cells.
		partWidth = ceilPowerOfTwo(width - (wholeX * c_tileSize));
		partHeight = ceilPowerOfTwo(height - (wholeY * c_tileSize));
		if (partWidth) {
			// create partial tiles along the right edge.
			int idx = tilesX - 1; // last tile in first row.
			for (iy=0; iy<wholeY; iy++) {
				f.grid[idx] = lf_createTile(f, partWidth, c_tileSize);
				idx += tilesX; // same place in next row.
			}
		}
		if (partHeight) {
			// create partial tiles along the bottom edge.
			int idx = tilesX * (tilesY - 1); // first tile in last row.
			for (ix=0; ix<wholeX; ix++) {
				f.grid[idx + ix] = lf_createTile(f, c_tileSize, partHeight);
			}
		}
		if (partWidth && partHeight) {
			// create the bottom right corner tile.
			f.grid[tilesX * tilesY - 1] = lf_createTile(f, partWidth, partHeight);
		}
	}
}

let MIN(a,b) = a < b ? a : b;

void lf_load_surface(LayerFrame f, SurfaceData* sd)
{
	int copyX, copyY, tilesX, ix, iy;
	copyX = (sd.width + (c_tileSize-1)) / c_tileSize;
	copyY = (sd.height + (c_tileSize-1)) / c_tileSize;
	if (copyX > f.tilesX) copyX = f.tilesX; // min.
	if (copyY > f.tilesY) copyY = f.tilesY; // min.
	tilesX = f.tilesX;
	for (iy=0; iy<copyY; iy++) {
		for (ix=0; ix<copyX; ix++) {
			GfxImage tile = f.grid[iy * tilesX + ix];
			if (tile) {
				// select the tile of the source surface to copy.
				int left = ix * c_tileSize, top = iy * c_tileSize;
				SurfaceData src = {
					sd.format,
					MIN(sd.width - left, c_tileSize), // keep tile size
					MIN(sd.height - top, c_tileSize), // within surface
					sd.stride,
					sd.data + (top * sd.stride) + (left * sd.format)
				};
				// upload this data as a sub-image of the tile.
				(*tile).update(tile, 0, 0, &src);
			}
		}
	}
}

// blend source image over destination image.
void f_blend_image(GfxImage dest, const iRect* destRect,
						  const FrameBlendImage* b) // ignores b.dest.
{
	SurfaceData sd = {0};
	SurfaceReadRGBA16 src;
	iPair size;

	// create a buffer for blending.
	// TODO: this should not be created in here each time!
	size = GfxImage_getSize(dest);
	assert(size.x > 0 && size.y > 0);
	surface_create(&sd, surface_rgba8, size.x, size.y);

	if(1)
	{
	// copy destination image to buffer.
	GfxImage_read(dest, &sd);

	// init reader from 16-bit surface.
	surface_read_rgba16(&src, b.image, b.alpha);

	// perform 16-bit blend op over 8-bit dest.
	surface_blend_source(&sd, destRect.left, destRect.top,
		&src.r, b.mode);

	// upload buffer back to destination image.
	GfxImage_update(dest, 0, 0, &sd);
	}

	surface_destroy(&sd);
}

// blend source image over all overlapping tiles.
void lf_blend_image(LayerFrame f, const FrameBlendImage* b)
{
	// determine which tiles overlap the destination rect (round out)
	int left = b.dest.left / c_tileSize;
	int top = b.dest.top / c_tileSize;
	int right = (b.dest.right + (c_tileSize-1)) / c_tileSize;
	int bottom = (b.dest.bottom + (c_tileSize-1)) / c_tileSize;
	int ix, iy;
	// only iterate over tiles inside document bounds.
	if (left < 0) left = 0;
	if (top < 0) top = 0;
	if (right > f.tilesX) right = f.tilesX;
	if (bottom > f.tilesY) bottom = f.tilesY;
	// iterate over tiles, blending to each one.
	for (iy=top; iy<bottom; iy++) {
		for (ix=left; ix<right; ix++) {
			GfxImage tile = f.grid[iy * f.tilesX + ix];
			// translate the dest rect into tile space.
			iRect dest = {
				b.dest.left - ix * c_tileSize, b.dest.top - iy * c_tileSize,
				b.dest.right - ix * c_tileSize, b.dest.bottom - iy * c_tileSize };
			// blend the source image to this tile.
			f_blend_image(tile, &dest, b);
		}
	}
}

int lf_message(Frame frame, FrameMessage msg, void* data)
{
	LayerFrame f = (LayerFrame)frame;
	switch (msg)
	{
	case frameRender:
		if (f.show) lf_draw(f, data);
		break;
	case frameSetRect:
		{fRect* rect = data;
		f.x = rect.left;
		f.y = rect.top;}
		break;
	case frameSetVisible:
		f.show = *(bool*)data;
		break;
	case frameGetVisible:
		*(bool*)data = f.show;
		break;
	case frameReleaseResources:
		lf_discard(f);
		break;
	case frameSetSize:
		{iPair* size = data;
		lf_resize(f, size.x, size.y);
		break;}
	case frameGetSize:
		((iPair*)data).x = f.width;
		((iPair*)data).y = f.height;
		break;
	case frameLoadSurfaceData:
		lf_load_surface(f, data);
		break;
	case frameBlendImage:
		lf_blend_image(f, data);
		break;
	case frameSetRenderContext:
		f.rc = data;
		break;
	}
	return 0;
}

export Frame frame_create_layer(Frame parent)
{
	LayerFrame frame = frame_alloc(sizeof(LayerFrame), lf_message);
	frame.grid = 0;
	frame.rc = 0; // TODO: hmm.
	frame.mode = gfxBlendPremultiplied;
	frame.alpha = 1.0f;
	frame.x = frame.y = 0;
	frame.width = frame.height = 0;
	frame.tilesX = frame.tilesY = 0;
	frame.show = true;
	frame_insert((Frame)frame, parent, -1); // append.
	return (Frame)frame;
}


// canvas frame.

struct CanvasFrame {
	Frame frame;
	Affine2D transform;
	int width, height;
	RGBA col; // paper colour.
	//RGBA bgcol; // outside paper.
	bool show;
};

void canvas_draw(CanvasFrame f, FrameRenderRequest* r)
{
	GfxDraw draw = r.draw;

	GfxDraw_blendMode(draw, gfxBlendCopy, 1);

	// save state so we can restore the view transform.
	GfxDraw_save(draw);

	// transform into canvas space.
	GfxDraw_transform(draw, &f.transform);

	// fill the canvas with opaque paper colour.
	GfxDraw_fillColor(draw, f.col.r/255.0f, f.col.g/255.0f, f.col.b/255.0f, 1.0f);
	GfxDraw_fillRect(draw, 0, 0, (float)f.width, (float)f.height);

	// render the canvas layers.
	frame_send_to_children(&f.frame, frameRender, r);

	// restore transform and other state.
	GfxDraw_restore(draw);
}

int canvas_message(Frame frame, FrameMessage msg, void* data)
{
	CanvasFrame f = (CanvasFrame)frame;
	switch (msg)
	{
	case frameRender:
		if (f.show) canvas_draw(f, data);
		break;
	case frameSetVisible:
		f.show = *(bool*)data;
		break;
	case frameGetVisible:
		*(bool*)data = f.show;
		break;
	case frameSetSize:
		{iPair* size = data;
		f.width = size.x; f.height = size.y;
		break;}
	case frameGetSize:
		((iPair*)data).x = f.width;
		((iPair*)data).y = f.height;
		break;
	case frameSetAffine:
		f.transform = *(Affine2D*)data;
		break;
	case frameGetAffine:
		*(Affine2D*)data = f.transform;
		break;
	}
	return 0;
}

export Frame frame_create_canvas(Frame parent)
{
	CanvasFrame frame = frame_alloc(sizeof(CanvasFrame), canvas_message);
	affine_set_identity(&frame.transform);
	frame.width = frame.height = 0;
	frame.col = rgba_white;
	//frame.bgcol = rgba_grey;
	frame.show = true;
	frame_insert((Frame)frame, parent, -1); // append.
	return (Frame)frame;
}


// ui frames

struct UIFrame {
	Frame frame;
	UI_Window* window;
	bool own;
};

int ui_message(Frame frame, FrameMessage msg, void* data)
{
	UIFrame uiframe = (UIFrame)frame;
	switch (msg)
	{
	case frameSetRect:
		break;
	case frameSetVisible:
		break;
	case frameParentChanged:
		if (uiframe.window) {
			// find the native parent window for this frame.
			UI_Window* parent = 0;
			frame_send_to_ancestors(frame, frameGetUIWindow, &parent);
			if (parent) {
				// re-parent this frame's native window.
				ui_reparent_window(uiframe.window, parent);
			}
		}
		break;
	case frameGetUIWindow:
		// return the native window attached to this frame.
		if (uiframe.window) {
			UI_Window** result = data;
			*result = uiframe.window;
			return 1; // stop walking up the hierarchy.
		}
		break;
	case frameReleaseResources:
		if (uiframe.own && uiframe.window)
			ui_destroy_window(uiframe.window);
		break;
	}
	return 0;
}

export Frame frame_create_ui_parent(UI_Window* window, bool own)
{
	UIFrame frame = frame_alloc(sizeof(UIFrame), ui_message);
	frame.window = window;
	frame.own = own; // destroy window when frame destroyed?
	return (Frame)frame;
}

export Frame frame_create_panel(Frame parent)
{
	UIFrame frame = frame_alloc(sizeof(UIFrame), ui_message);
	frame.window = 0;
	frame.own = true; // destroy window when frame destroyed.
	return (Frame)frame;
}

export Frame frame_create_button(Frame parent)
{
	UIFrame frame = frame_alloc(sizeof(UIFrame), ui_message);
	frame.window = 0;
	frame.own = true; // destroy window when frame destroyed.
	return (Frame)frame;
}



/*

// NB this is just testing compiler support for SSE2.

#include <emmintrin.h>

void bit_operation_and(unsigned* dst, const unsigned* src, unsigned block_size)
{
      const __m128i* wrd_ptr = (__m128i*)src;
      const __m128i* wrd_end = (__m128i*)(src + block_size);
      __m128i* dst_ptr = (__m128i*)dst;

      do
      {
           __m128i xmm1 = _mm_load_si128(wrd_ptr);
           __m128i xmm2 = _mm_load_si128(dst_ptr);

           xmm1 = _mm_and_si128(xmm1, xmm2);     //  AND  4 32-bit words
           _mm_store_si128(dst_ptr, xmm1);
           ++dst_ptr;
           ++wrd_ptr;

      } while (wrd_ptr < wrd_end);
}

*/



/*

// This was the span source renderer for canvas frame.

struct AffineRect {
	int y, end; // current y, final y.
	int l, lc, dl; // left: x, corner-y, delta-x
	int r, rc, dr; // right: x, corner-y, delta-x
	int ofs; // x within current span.
};

struct CanvasSource {
	SpanSource source;
	AffineRect shape;
	Frame frame;
	RGBA border, paper;
	SpanBuffer buffer;
};

bool canvas_more(SpanSource* source)
{
	CanvasSource* cs = (CanvasSource*)source;
	cs.shape.y++; // always advance one raster.
	cs.shape.ofs = 0; // reset rendering offset within raster.
	if (cs.shape.y < cs.shape.lc) cs.shape.l += cs.shape.dl; else cs.shape.l += cs.shape.dr;
	if (cs.shape.y < cs.shape.rc) cs.shape.r += cs.shape.dr; else cs.shape.r += cs.shape.dl;
	return true; // canvas renders an infinite surface.
}

byte* canvas_read(SpanSource* source, int len)
{
	CanvasSource* cs = (CanvasSource*)source;
	byte* dest = cs.buffer.data;
	int ofs, avail;
	assert(len >= 0);
	ofs = cs.shape.ofs; cs.shape.ofs += len;
	// fill span left of the canvas.
	avail = cs.shape.l - ofs;
	if (avail > len) avail = len;
	if (avail > 0) {
		span4_col_copy(dest, avail, cs.border);
		len -= avail;
		ofs += avail;
		dest += avail << 2;
	}
	// fill span inside the canvas.
	avail = cs.shape.r - ofs;
	if (avail > len) avail = len;
	if (avail > 0) {
		span4_col_copy(dest, avail, cs.paper);
		frame_send_to_children(cs.frame, frame, &req);
		len -= avail;
		ofs += avail;
		dest += avail << 2;
	}
	// fill span to the right of the canvas.
	if (len > 0) {
		span4_col_copy(dest, len, cs.border);
	}
	// return our internal buffer.
	return cs.buffer.data;
}

*/
