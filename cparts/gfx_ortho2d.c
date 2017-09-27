#include "defs.h"
#include "graphics.h"
#include "gl_impl.h"
#include "surface.h"


typedef struct GfxOrtho2D {
	GfxScene_i* scene_i; // for upstream; first member fast path.
	GfxContext gctx; // from upstream scene.
	GfxScene scene; // downstream scene to render.
	GfxCol bgcol; // background clear colour.
} GfxOrtho2D;

#define GfxSceneToSelf(PTR) impl_cast(GfxOrtho2D, scene_i, (PTR))


static void go2d_init(GfxScene ifptr, GfxContext gctx)
{
	GfxOrtho2D* self = GfxSceneToSelf(ifptr);

	self->gctx = gctx;

	// don't need perspecive correction for 2D.
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);

	// don't care which way triangles are wound.
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); // the default.
	glDisable(GL_CULL_FACE);

	// pass downstream.
	(*self->scene)->init(self->scene, gctx);
}

static void go2d_final(GfxScene ifptr)
{
	GfxOrtho2D* self = GfxSceneToSelf(ifptr);

	// destroy context resources.

	// pass downstream.
	(*self->scene)->final(self->scene);
}

static void go2d_sized(GfxScene ifptr, int width, int height)
{
	GfxOrtho2D* self = GfxSceneToSelf(ifptr);

	// set up an orthographic projection for 2D rendering.
	// TODO should be able to specify a rect?
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, width, height, 0, 1, -1);
	glMatrixMode(GL_MODELVIEW);

	// pass downstream.
	(*self->scene)->sized(self->scene, width, height);
}

static void go2d_render(GfxScene ifptr, GfxRender render)
{
	GfxOrtho2D* self = GfxSceneToSelf(ifptr);

	// TODO this should be an optional operation.
	glClearColor(self->bgcol.r, self->bgcol.g, self->bgcol.b, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	glLoadIdentity();

	// pass downstream.
	(*self->scene)->render(self->scene, render);
}

static void go2d_update(GfxScene ifptr, GfxDelta delta)
{
	GfxOrtho2D* self = GfxSceneToSelf(ifptr);

	// pass downstream.
	(*self->scene)->update(self->scene, delta);
}

static GfxScene_i go2d_scene_i = {
	go2d_init,
	go2d_final,
	go2d_sized,
	go2d_render,
	go2d_update,
};

GfxScene createGfxOrtho2D(GfxScene scene)
{
	GfxOrtho2D* self = cpart_new(GfxOrtho2D);
	self->scene_i = &go2d_scene_i;
	self->gctx = 0; // until init is called.
	self->scene = scene;
	self->bgcol.r = self->bgcol.g = self->bgcol.b = 0.5f;
	self->bgcol.a = 1.0f;
	return &self->scene_i;
}
