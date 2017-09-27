#include "defs.h"
#include "layers.h"
#include "surface.h"

struct Layer {
	SurfaceLayerRenderFunc render;
	SurfaceLayerEventFunc event;
	Transform t;
};

struct SurfaceLayer {
	Layer layer;
	SurfaceData sd;
};

typedef enum GfxBlendMode { compositeBlend=1 } GfxBlendMode;
typedef struct CompositeMode { GfxBlendMode mode; double alpha; } CompositeMode;


// SurfaceLayer

static void surface_layer_render(Layer* layer, RenderRequest* req)
{
	SurfaceLayer* sl = (SurfaceLayer*)layer;
	CompositeMode mode = { compositeBlend, 1 };
	irect rect = { 0, 0, sl->sd.width, sl->sd.height };
	//req->ir->image(req->obj, &sl->sd, rect, mode);
}

static void surface_layer_event(Layer* layer, LayerEvent* e)
{
}

SurfaceLayer* surface_layer_create(size_t width, size_t height)
{
	SurfaceLayer* sl = cpart_new(SurfaceLayer);
	sl->layer.render = surface_layer_render;
	sl->layer.event = surface_layer_event;
	sl->layer.t.scale = 1;
	surface_create(&sl->sd, width, height);
	surface_fill(&sl->sd, rgba_black);
	return sl;
}


/*
// ButtonLayer

static void button_layer_render(Layer* layer, RenderRequest* req)
{
	ButtonLayer* l = (ButtonLayer*)layer;
	Rect rect = { 0, 0, l->size.x, l->size.y };
	if (l->is_down && l->style->down)
		req->ir->image(req->obj, l->style->down, rect, l->style->mode);
	else if (l->style->up)
		req->ir->image(req->obj, l->style->up, rect, l->style->mode);
	if (l->img)
		req->ir->image(req->obj, l->img, rect, l->style->mode);
	if (l->hover && l->style->over)
		req->ir->image(req->obj, l->style->over, rect, l->style->overmode);
}

static void button_invalidate(ButtonLayer* l)
{
	LayerEvent e; e.event = layerInvalidate;
	e.pos = untransform(&l->layer.t, 0, 0);
	e.ext = untransform(&l->layer.t, l->size.x, l->size.y);
	if (l->layer.parent) l->layer.parent->notify(l->layer.parent, &e);
}

static void button_layer_event(Layer* layer, LayerEvent* e)
{
	ButtonLayer* l = (ButtonLayer*)layer;
	switch (e->event)
	{
	case layerOver: l->hover = true; button_invalidate(l); break;
	case layerOut: l->hover = false; button_invalidate(l); break;
	case layerDown: l->is_down = true; button_invalidate(l); break;
	case layerUp: { bool down = l->is_down; l->is_down = false;
		button_invalidate(l); if (down && l->click.func) {
			LayerEvent ce; ce.event = layerClicked; ce.pos = e->pos;
			l->click.func(l->click.obj, &ce); } }
		break;
	}
}

ButtonLayer* button_layer_create(ButtonStyle* style, Point size, SurfaceData* img)
{
	ButtonLayer* l = cpart_new(ButtonLayer);
	l->layer.render = button_layer_render; l->layer.event = button_layer_event;
	l->layer.t.scale = 1;
	l->style = style; l->size = size; l->img = img;
	return l;
}


// ToolbarLayer

void toolbar_layer_create(ToolbarLayer* l, Point size)
{
}
*/


// Utils

dPoint untransform(Transform* t, double x, double y)
{
	dPoint r = { (1.0 / t->scale) * (x - t->org.x), (1.0 / t->scale) * (y - t->org.y) };
	return r;
}

dPoint transform(Transform* t, dPoint pt)
{
	dPoint r= { (pt.x * t->scale) + t->org.x, (pt.y * t->scale) + t->org.y };
	return r;
}
