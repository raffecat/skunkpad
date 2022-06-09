#include "defs.h"
#include "app.h"
#include "ui.h"
#include "frames.h"
#include "surface.h"
#include "tablet_input.h"
#include "lib_png.h"
#include "lib_jpeg.h"
#include "blend.h"
#include "res_load.h"
#include "pagebuf.h"
#include "skunkpad.h"
#include "shapes.h"
#include "timer.h"
#include "glview.h"
#include "draw.h"
#include "pancontrol.h"

#include <math.h>
#include <limits.h>

#include "str.h"
#include "surface.h"
#include "frames.h"

// api for lua bindings.
string browse_file_open(stringref caption, stringref path);
string browse_file_save(stringref caption, stringref path);
void new_doc(int width, int height);
void open_doc(stringref path);
void save_doc(stringref path);
void close_doc();
void new_layer(int above);
void delete_layer(int index);
void show_layer(int index, bool show);
void load_into_layer(int index, stringref path);
void active_layer(int index); // for painting.
void set_brush_mode(BlendMode mode);
void set_brush_size(int sizeMin, int sizeMax, int spacing);
void set_brush_alpha(int alphaMin, int alphaMax);
void set_brush_col(RGBA col);
void begin_painting();
void invalidate_all();
void zoom(int steps);
void resetZoom();

// timers
void* start_timer(unsigned long delay, unsigned long interval);
void stop_timer(void* timer);


// overlay frames.

Frame* create_frame(Frame* parent, int after);
void destroy_frame(Frame* frame);
void insert_frame(Frame* frame, Frame* parent, int after);
void set_frame_col(Frame* frame, float red, float green, float blue, float alpha);
void set_frame_alpha(Frame* frame, float alpha);
void set_frame_image(Frame* frame, GfxImage image);
void invalidate_frame(Frame* frame);
GfxImage load_image(stringref path);
Frame* get_layer(int index);

extern Frame* g_root_frame;


/*
void move_frame(Frame* frame, float left, float top, float right, float bottom);
void set_frame_colour(Frame* frame, float r, float g, float b, float a);
void set_frame_image(Frame* frame, Image* image);
void set_frame_coords(Frame* frame, float left, float top, float right, float bottom);
void set_frame_border(int frame, float thickness, float dash, float r, float g, float b, float a);
void set_frame_shape(int frame, float radius); // non-zero for round rect.
*/


// undo

typedef struct UndoBuffer UndoBuffer;
typedef void (*UndoBufferApply)(void* obj, void* data, int size);
UndoBuffer* undobuf_create(size_t size);
void undobuf_resize(UndoBuffer* ub, size_t size);
void undobuf_begin(UndoBuffer* ub, void* data, int size); // begin a packet.
void undobuf_append(UndoBuffer* ub, void* data, int size); // append to packet.
void undobuf_end(UndoBuffer* ub); // end of current packet.
void undobuf_undo(UndoBuffer* ub, UndoBufferApply func, void* obj);
void undobuf_redo(UndoBuffer* ub, UndoBufferApply func, void* obj);

void undo(UndoBuffer* ub);
void redo(UndoBuffer* ub);

// app stuff that undo uses...
extern Frame* activeLayer;
void update_brush();


// brush engine.
struct Tablet_InputEvent;
typedef struct DabPainter DabPainter;
// merge deferred painting into the document.
// org: top-left corner of source rect (size from bounds)
// bounds: destination rect in document space.
typedef void (*DabPainterOutput)(void* obj, SurfaceData* image, iPair org, iRect bounds);
DabPainter* createDabPainter(GfxDraw draw);
//void painter_set_context(DabPainter* dp, GfxContext cx);
void painter_begin(DabPainter* dp, struct Tablet_InputEvent* e);
void painter_begin_batch(DabPainter* dp);
void painter_draw(DabPainter* dp, struct Tablet_InputEvent* e);
void painter_end_batch(DabPainter* dp);
void painter_end(DabPainter* dp);
void painter_set_brush(DabPainter* dp, SurfaceData* brush);
void painter_set_colour(DabPainter* dp, RGBA col);
void painter_set_alpha_range(DabPainter* dp, int min, int max);
// specify minimum and maximum brush size in Q8 document coords.
// the brush shape is always square for now.
void painter_set_size_range(DabPainter* dp, int min, int max);
// specify dab spacing in Q8 document coords.
void painter_set_spacing(DabPainter* dp, int ratio);
// set a callback that will merge deferred paint into the document.
void painter_set_output(DabPainter* dp, DabPainterOutput func, void* obj);
//GfxImage painter_get_accum(DabPainter* dp);
//RGBA painter_get_col(DabPainter* dp);


// bindings
void init_bindings();
void term_bindings();
void notify_resize(int w, int h);
void notify_pointer(int x, int y, int buttons);
void notify_wheel(int x, int y, int delta);
void notify_scroll(int x, int y);
void notify_key(int key, int down);
void notify_timer(void* timer);
void unreg_frame(Frame* frame);
void unreg_timer(void* timer);


// utils
RGBA make_rgba(float r, float g, float b, float a);



typedef struct ScaledView ScaledView;
struct ScaledView {
    iPair extent, border;
    int zoom; // percent zoom * 120.
    float scale;
    float anti; // 1/scale, the antiscale.
};

typedef struct SkunkDoc SkunkDoc;
struct SkunkDoc {
    Frame* layers;
    int width, height;
    RGBA paper;
};

// public for pancontrol.
iPair scrollPos = {0,0};
UI_Window* scrollView = 0;

static UI_Window* mainWnd = 0;
static GLView* glView = 0;
static GfxContext gfxContext = 0;
static GfxDraw gfxDraw = 0;
static iRect drawRect = {0};
static TabletInput* tablet = 0;
static ScaledView scaledView = {0};
static SkunkDoc* document = 0;
Frame* activeLayer = 0; // public for undo.
static int activeLayerIndex = 0;
static bool paintMode = false;
static bool penIsDown = false;
static DabPainter* painter = 0;
static SurfaceData brush = {0};
static BlendMode brushMode = blendNormal;
static bool needCommit = false;
static timer_t* commitTimer = 0;
UndoBuffer* undoBuf = 0; // public for undo, bindings.
Frame* g_root_frame = 0; // public for bindings.
static Frame* g_background = 0;
static int g_wheelAccum = 0;
static Affine2D g_viewToDoc = {0};
static Affine2D g_docToView = {0};

static bool load_image_sd(stringref path, SurfaceData* sd, bool premultiply)
{
    dataBuf buf;

	assert(sd->format == surface_invalid);

    buf = res_load(path);
    if (buf.size) {
        load_png_buf(sd, buf);
        buf.free(&buf);
    }

    if (!surfaceValid(sd))
        return 0;

    if (premultiply)
		surface_premultiply(sd);

    return true;
}

void show_frame(Frame* frame, bool show) {
    frame->message(frame, frameSetVisible, &show);
}

void frame_set_rect(Frame* frame, float left, float top, float right, float bottom) {
    fRect rect = { left, top, right, bottom };
    frame->message(frame, frameSetRect, &rect);
}

void frame_set_size(Frame* frame, int width, int height) {
    iPair size = { width, height };
    frame->message(frame, frameSetSize, &size);
}

void frame_set_affine(Frame* frame, Affine2D_in transform) {
    frame->message(frame, frameSetAffine, (void*)transform);
}

static void updateCanvasTransform()
{
    ScaledView* sv = &scaledView;
    if (document) {
        // position the document inside a border box.
        iPair org = scrollPos;
        org.x -= sv->border.x;
        org.y -= sv->border.y;
        // this one is still wrong.
        // viewToDoc: translate client to view-extent, then (un)scale.
        // org is (x>=0,y>=0) position inside the view extent.
        affine_set_scale(sv->anti, sv->anti, &g_viewToDoc);
        affine_pre_translate(&g_viewToDoc, (float)org.x, (float)org.y, &g_viewToDoc);

        // this one seems right now...
        // docToView: scale to view space, then translate to client.
        affine_set_scale(sv->scale, sv->scale, &g_docToView);
        affine_post_translate(&g_docToView, -(float)org.x, -(float)org.y, &g_docToView);
        // rendering projects document coords to view space.
        frame_set_affine(document->layers, &g_docToView);
    }
    else {
        affine_set_identity(&g_viewToDoc);
        affine_set_identity(&g_docToView);
    }
    //trace("updated canvas transform");
}

static const int c_minZoom = 1*120;    // percent
static const int c_maxZoom = 1600*120; // percent

static void updateCanvasScale(int zoom)
{
    if (zoom < c_minZoom) zoom = c_minZoom;
    else if (zoom > c_maxZoom) zoom = c_maxZoom;
    scaledView.zoom = zoom;
    scaledView.scale = scaledView.zoom / (100.0f * 120);
    scaledView.anti = 1.0f / scaledView.scale;
}

static void updateCanvasExtent()
{
    int ex=0, ey=0;

    if (document) {
        // TODO: even at 4100x4100 this breaks when we zoom in
        // beyond about 1200% - this isn't going to work for
        // large documents and large zooms.
        ex = (int)(document->width * scaledView.scale);
        ey = (int)(document->height * scaledView.scale);

        // add 2x border size to the extent.
        ex += scaledView.border.x * 2;
        ey += scaledView.border.y * 2;
    }

    scaledView.extent.x = ex;
    scaledView.extent.y = ey;
}

static void setZoom(int zoom, iPair pos)
{
    fPair pt = { (float)pos.x, (float)pos.y };
    fPair dp, mp;
    iPair newPos;

    // map client pos to doc space using current zoom.
    affine_transform(&g_viewToDoc, (float*)&pt, (float*)&dp, 1);

    updateCanvasScale(zoom);
    updateCanvasExtent();
    updateCanvasTransform();

    // map doc point back to view space using new transform.
    affine_transform(&g_docToView, (float*)&dp, (float*)&mp, 1);

    // when zooming in, doc extent becomes larger; the same point
    // in client space maps to a point closer to the doc origin.
    newPos.x = scrollPos.x + (int)(mp.x - pt.x);
    newPos.y = scrollPos.y + (int)(mp.y - pt.y);

    //trace("adjusting the scroll extent and position...");
    ui_scroll_view_set_extent_pos(scrollView, scaledView.extent, newPos);
    //trace("... finished adjusting scroll extent and position");

    // redraw the whole view.
    //trace("invalidating view at end of zoom");
    ui_invalidate(scrollView);
}

static void adjustZoom(int delta, iPair pos)
{
    int zoom = scaledView.zoom + delta;
    if (delta > 0) zoom += zoom / 20;
    else zoom -= zoom / 20;
    setZoom(zoom, pos);
}

void zoom(int steps) // Lua API
{
    iPair pos = ui_get_pointer_pos();
    adjustZoom(steps * 120, pos);
}

void resetZoom() // Lua API
{
    iPair pos = ui_get_pointer_pos();
    setZoom(100 * 120, pos);
}

static void main_handler(void* context, UI_Event* e) {
    switch (e->event)
    {
    case ui_event_size: {
        UI_SizeEvent* ev = (UI_SizeEvent*)e;
        if (scrollView) ui_size_window(scrollView, ev->width, ev->height);
        break; }
    }
}

static void draw_scene(void* data)
{
	static const GfxCol c_grey_background = { 0.5f, 0.5f, 0.5f, 1.0f };
	GfxDraw draw = gfxDraw;
    FrameRenderRequest req;
	fRect clip = { 0, 0, (float)drawRect.right, (float)drawRect.bottom };
    req.draw = draw;
	req.clip = &clip;
    req.alpha = 1;

	GfxDraw_begin(draw);
	GfxDraw_clear(draw, c_grey_background);

	// fill the background with grey.
	// GfxDraw_fillColor(draw, 0.5f, 0.5f, 0.5f, 1.0f);
	// GfxDraw_fillRect(draw, 0, 0, (float)drawRect.right, (float)drawRect.bottom);

	if (g_root_frame)
		g_root_frame->message(g_root_frame, frameRender, &req);

	/*
	{SurfaceData* img = painter_get_accum(painter);
	 if (img) {
		GfxDraw_blendMode(draw, gfxBlendCopy, 1.0f);
		GfxDraw_drawImage(draw, img, 200, 20);

		GfxDraw_fillColor(draw, 0, 0, 0, 0);
		GfxDraw_blendMode(draw, gfxBlendPremultiplied, 1.0f);

		GfxDraw_drawImage(draw, img, 200+128+20, 20);
	}}
	*/

	GfxDraw_end(draw);
}

static void view_handler(void* context, UI_Event* e) {
    switch (e->event) {
        case ui_event_focus:
            tablet_activate(tablet);
            break;
        case ui_event_size: {
            UI_SizeEvent* ev = (UI_SizeEvent*)e;
			//trace("event resize start");
            scaledView.border.x = ev->width - (ev->width/10);
            scaledView.border.y = ev->height - (ev->height/10);
            // FIXME: scroll the view by the amount that the border
            // has changed :(

			// TODO: the logic here is horrible; setZoom causes scrollView
			// extent to change, which causes up to two re-entrant ui_event_size
			// to arrive while still processing the first one (only when
			// scroll bars are hidden or shown though.)

			// This means the UI_SizeEvent is out of date after the setZoom
			// call (for non-innermost calls) and therefore we need to
			// notify interested parties before calling setZoom.
			// Better would be to avoid the re-entrant behaviour.

			drawRect.right = ev->width;
			drawRect.bottom = ev->height;
            if (g_root_frame) {
                notify_resize(ev->width, ev->height);
            }
            if (scrollView) { // must do this one last.
                iPair origin = {0,0};
                setZoom(scaledView.zoom, origin);
			}
			//trace("event resize end");
            break; }
        case ui_event_pointer:
            //{UI_PointerEvent* ev = (UI_PointerEvent*)e;
            // trace("pointer %d, %d (%d)", ev->pos.x, ev->pos.y, ev->buttons);}
            if (paintMode) {
                // route pointer input through the tablet.
                tablet_input_process(tablet, (UI_PointerEvent*)e);
            }
            else {
                // pass pointer event to ui handler.
                UI_PointerEvent* ev = (UI_PointerEvent*)e;
                notify_pointer(ev->pos.x, ev->pos.y, ev->buttons);
                // ui might have started drawing.
                if (paintMode) {
                    // route pointer input through the tablet.
                    tablet_input_process(tablet, ev);
                }
            }
            break;
        case ui_event_wheel: {
            UI_WheelEvent* ev = (UI_WheelEvent*)e;
            adjustZoom(ev->delta, ev->pos);
            break; }
        case ui_event_key: {
            UI_KeyEvent* ev = (UI_KeyEvent*)e;
			if (!panKeyInput(ev))
                notify_key(ev->key, ev->is_down);
            break; }
    }
}

static void scroll_handler(void* context, UI_Event* e) {
    switch (e->event) {
        /*case ui_event_paint:
			// use the software renderer to draw the scene.
			if (rendToView) {
				UI_PaintEvent* ev = (UI_PaintEvent*)e;
				ui_render_to_view(rendToView, ev, draw_scene, 0);
			}
            break;*/
        case ui_event_focus:
			if (glView)
				ui_focus_window((UI_Window*)glView);
			/*else if (rendToView)
	            tablet_activate(tablet);*/
            break;
        case ui_event_size: {
            UI_SizeEvent* ev = (UI_SizeEvent*)e;
            if (glView) {
                glview_resize(glView, ev->width, ev->height);
            } /*else {
				view_handler(context, e);
			}*/
            break; }
        case ui_event_scroll: {
            UI_SizeEvent* ev = (UI_SizeEvent*)e;
            // TODO: update zoom_view.
            // width/height are scroll position.
            //trace("received a scroll event");
            scrollPos.x = ev->width;
            scrollPos.y = ev->height;
            updateCanvasTransform();
            // redraw the whole view.
            //trace("invalidating in scroll event");
            ui_invalidate(scrollView);
            break; }
        /*case ui_event_pointer:
        case ui_event_wheel:
        case ui_event_key:
			if (rendToView)
				view_handler(context, e);
			break;*/
    }
}

static void tablet_handler(void* context, Tablet_Event* ev) {
    // TODO: check active tool and route (e.g. select)
    // TODO: check modifiers and route (e.g. panning key)
    switch (ev->event)
	{
	case tablet_event_begin:
		if (penIsDown)
			painter_begin_batch(painter);
		break;
	case tablet_event_end:
		if (penIsDown)
			painter_end_batch(painter);
		break;
	case tablet_event_input:
		{
			Tablet_InputEvent* e = (Tablet_InputEvent*)ev;

			if (panMode) {
				panPenInput(e);
			}
			else
			{
				// map sample point to document space.
				fPair pt = { (float)e->x, (float)e->y };
				affine_transform(&g_viewToDoc, (float*)&pt, (float*)&pt, 1);
				e->x = pt.x; e->y = pt.y;

				if (penIsDown) {
					if (e->buttons) {
						// pen is still down.
						painter_draw(painter, e);
					}
					else {
						// pen is up - finish drawing.
						painter_end(painter);
						paintMode = false; // stop painting.
						penIsDown = false;
					}
				}
				else {
					if (e->buttons) {
						// pen went down - begin drawing.
						penIsDown = true;
						painter_begin(painter, e);
					}
				}
			}
		}
		break;
	}
}

/*static void invalidate(void* obj, iRect rect)
{
    float r[4] = { (float)rect.left, (float)rect.top,
                   (float)(rect.right+1), (float)(rect.bottom+1) };
    affine_transform(&g_docToView, r, r, 2);
    ui_invalidate_rect(scrollView, (int)r[0]-1, (int)r[1]-1, (int)r[2], (int)r[3]);
}*/


/*
	self->blendBuffer = GfxContext_createTarget(gc, imgFormatARGB8,
		128, 128, 0, gfxTargetHasColour);

	self->modeNormal = GfxContext_createShader(gc);
	GfxShader_setBlendFunc(self->modeNormal, gfxSrcAlpha, gfxOneMinusSrcAlpha);	
*/

/*
static void paint_tile(void* data, Frame* frame, int x, int y, GfxImage tile) {
	DabPainter* dp = data;
	// we're given the x and y coordinates of the tile relative to
	// the requested rect (and therefore in document space.)
	painter_merge_to(dp, x, y, tile);
}
*/

static void paint_output(void* data, SurfaceData* image, iPair org, iRect bounds) {
	if (activeLayer) {
		FrameBlendImage bi;
		bi.mode = brushMode;
		bi.alpha = 255;
		bi.image = image;
		// source rect at org (top-left within source image)
		// the same size as the dest rect (bounds)
		bi.source.left = org.x; bi.source.top = org.y;
		bi.source.right = org.x + (bounds.right - bounds.left);
		bi.source.bottom = org.y + (bounds.bottom - bounds.top);
		bi.dest = bounds;
		// flatten the deferred drawing into each tile from the active
		// layer that overlaps the bounds.
		activeLayer->message(activeLayer, frameBlendImage, &bi);
		// repaint the view.
		invalidate_all();
		needCommit = true;
	}
}

static void flush_output(void* data, timer_t* timer) {
	if (needCommit && scrollView) {
		ui_commit_window(scrollView);
		needCommit = false;
	}
}

void init()
{
	app_heap_check();
    app_set_scheduler(timer_run, 0);

    mainWnd = ui_create_app_window("Skunkpad", main_handler, 0);
    scrollView = ui_create_scroll_view(mainWnd, scroll_handler, 0);

    tablet = tablet_input_create(scrollView, tablet_handler, 0);
    glView = glview_create(scrollView, view_handler, 0);
	gfxContext = glview_get_context(glView);
	gfxDraw = createGfxDraw(gfxContext);

	// must create the painter before scene init and before
	// any tablet events can arrive [too many global deps!]
    painter = createDabPainter(gfxDraw);
	painter_set_output(painter, paint_output, 0);

	{stringref file = str_lit("round_brush.png");
	load_image_sd(file, &brush, false);
	painter_set_brush(painter, &brush);}

    // make the background shown when no document is loaded.
    g_root_frame = frame_create_box(0);
    g_background = frame_create_box(g_root_frame);
    set_frame_col(g_background, 0.5f, 0.5f, 0.5f, 1.0f);
    frame_set_rect(g_background, 0, 0, 65535, 65535);

	glview_set_scene(glView, draw_scene, 0);

    scaledView.zoom = 100*120;

    undoBuf = undobuf_create(50 * 1024 * 1024); // 50 Mb.

    init_bindings();

    ui_center_window(mainWnd);
    ui_show_window(mainWnd, true);

	commitTimer = timer_add(100, 100, flush_output, 0);
}

void run() {}

void final()
{
    if (tablet) tablet_input_destroy(tablet);
    term_bindings();
	frame_destroy(g_root_frame);
	g_root_frame = 0;
}


// ------------------------- helpers -------------------------

RGBA make_rgba(float r, float g, float b, float a)
{
    RGBA col;
    r=r*256.0f; g=g*256.0f; b=b*256.0f; a=a*256.0f;
    col.r = (r<0) ? 0 : ((r>255) ? 255 : (int)r);
    col.g = (g<0) ? 0 : ((g>255) ? 255 : (int)g);
    col.b = (b<0) ? 0 : ((b>255) ? 255 : (int)b);
    col.a = (a<0) ? 0 : ((a>255) ? 255 : (int)a);
    return col;
}

Frame* get_layer(int index)
{
    if (document)
        return frame_child(document->layers, index - 1);
    return 0;
}

static void no_active_layer()
{
    activeLayer = 0;
    activeLayerIndex = 0;
}

static void no_document()
{
    document = 0;
    no_active_layer();
}


// ------------------------- API -------------------------

void close_doc()
{
    iPair org = {0,0};
    // free all document memory.
    if (document) {
        // free all layers.
        destroy_frame(document->layers);
        document->layers = 0;
        // free the document.
        cpart_free(document);
        no_document();
    }
    // clear scroll extent to hide scrollbars.
    ui_scroll_view_set_extent(scrollView, org);
    // show the background layer.
    show_frame(g_background, true);
    ui_invalidate(scrollView);
}

void new_doc(int width, int height)
{
    iPair org = {0,0};
    // make sure there is no document first.
    close_doc();
    // create a new document.
    document = cpart_new(SkunkDoc);
    document->width = width;
    document->height = height;
    document->paper = rgba_white;
    // create the canvas frame.
    document->layers = frame_create_canvas(g_root_frame);
    frame_set_size(document->layers, width, height);
    // ensure canvas is drawn first.
    frame_insert(document->layers, g_root_frame, 0);
    // reset zoom and update extent.
    setZoom(100*120, org);
    // scroll to the top-left corner of the canvas.
    org = scaledView.border;
    if (org.x > 8) org.x -= 8;
    if (org.y > 8) org.y -= 8;
    ui_scroll_view_set_pos(scrollView, org);
    // hide background, use canvas instead.
    show_frame(g_background, false);
    ui_invalidate(scrollView);
}

void new_layer(int above)
{
    if (document) {
        // create a new layer.
        Frame* layer = frame_create_layer(document->layers);
		// TODO: frames should belong to a FrameScene with a GfxContext.
		layer->message(layer, frameSetRenderContext, gfxContext);
        frame_set_size(layer, document->width, document->height);
        if (above >= 0)
            insert_frame(layer, document->layers, above); // change Z order.
    }
}

void delete_layer(int index)
{
    // find the layer directly below the one to delete.
    Frame* layer = get_layer(index);
    if (layer) {
        // clear selection if layer is selected.
        if (layer == activeLayer)
            no_active_layer();
        // destroy the layer.
        // TODO: move to undo buffer instead.
        destroy_frame(layer);
    }
}

void load_into_layer(int index, stringref path)
{
    Frame* layer = get_layer(index);
    if (layer) {
        SurfaceData sd;
        if (load_png(&sd, strr_cstr(path))) {
            layer->message(layer, frameLoadSurfaceData, &sd);
            surface_destroy(&sd);
        }
    }
}

GfxImage load_image(stringref path) //, bool premultiply)
{
    SurfaceData sd = {0};
    GfxImage img;

	if (!load_image_sd(path, &sd, false))
        return 0;

	if (gfxContext) {
		img = GfxContext_createImage(gfxContext);
		GfxImage_upload(img, &sd, 0);
	}

	surface_destroy(&sd);

    return img;
}

void insert_frame(Frame* frame, Frame* parent, int after)
{
    frame_insert(frame, parent, after);
}

Frame* create_frame(Frame* parent, int after)
{
    Frame* frame = frame_create_box(parent);
    if (after >= 0)
        insert_frame(frame, parent, after); // change Z order.
    return frame;
}

static void recurs_unreg_frame(Frame* frame)
{
    Frame* walk = frame->children;
    unreg_frame(frame);
    while (walk) { recurs_unreg_frame(walk); walk = walk->next; }
}

void destroy_frame(Frame* frame)
{
    recurs_unreg_frame(frame);
    frame_destroy(frame);
}

void set_frame_col(Frame* frame, float red, float green, float blue, float alpha)
{
    RGBA col = make_rgba(red, green, blue, alpha);
    frame->message(frame, frameSetColour, &col);
}

void set_frame_alpha(Frame* frame, float alpha)
{
    frame->message(frame, frameSetAlpha, &alpha);
}

void set_frame_image(Frame* frame, GfxImage image)
{
    frame->message(frame, frameSetImage, image);
}

void invalidate_frame(Frame* frame)
{
    // TODO: project frame rect to client space.
    ui_invalidate(scrollView);
}

void invalidate_all()
{
    ui_invalidate(scrollView);
}

void set_brush_mode(BlendMode mode)
{
	brushMode = mode;
}

void set_brush_size(int sizeMin, int sizeMax, int spacing) // Q8
{
    if (sizeMin > 255*256) sizeMin = 255*256;
    if (sizeMax > 255*256) sizeMax = 255*256;
    if (spacing > 255*256) spacing = 255*256;
    painter_set_size_range(painter, sizeMin, sizeMax);
	painter_set_spacing(painter, spacing);
}

void set_brush_alpha(int alphaMin, int alphaMax) // Q8
{
    painter_set_alpha_range(painter, alphaMin, alphaMax);
}

void set_brush_col(RGBA col)
{
    painter_set_colour(painter, col);
}

void begin_painting()
{
    paintMode = true;
}

void active_layer(int index)
{
    Frame* layer = get_layer(index);
    // select new layer for drawing.
    if (layer) {
        activeLayer = layer;
        activeLayerIndex = index;
    } else no_active_layer();
}

static void cb_timer(void* data, timer_t* timer)
{
    notify_timer(timer); // send notification to Lua.
}

void* start_timer(unsigned long delay, unsigned long interval)
{
    return timer_add(delay, interval, cb_timer, 0);
}

void stop_timer(void* timer)
{
    timer_remove(timer);
}
