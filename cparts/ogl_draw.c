#include "defs.h"
#include "draw.h"
#include "gl_impl.h"

#include <float.h>

#define NOT_NAN(X) ((X)==(X))
#define NOT_INF(X) ((X)<FLT_MAX)


// OpenGL backend for GfxDraw.
// "Because one layer of abstraction is enough."

typedef struct OGLDrawState {
	Affine2D transform;
	fRect clip;
	GfxCol fillCol;
	GfxCol strokeCol;
	float alpha;
	float lineWidth;
	float miterLimit;
	GfxCapStyle lineCap;
	GfxJoinStyle lineJoin;
	GfxBlendMode mode;
} OGLDrawState;

typedef struct OGLDrawImpl {
	GfxDraw_i* draw_if;		// first member fast path.
	GfxContext cx;			// upstream GfxContext.
	GfxRender r;			// upstream GfxRender from GfxContext.
	GfxBlendMode mode;		// active blending mode.
	bool transformDirty;    // transform has been modified.
	OGLDrawState* ds;		// active drawing state on the stack.
	OGLDrawState* stack;	// stack of saved drawing states.
	unsigned int stackTop;  // index of active drawing state.
	unsigned int stackSize; // number of OGLDrawState allocated.
} OGLDrawImpl;

#define GfxDrawToGLImpl(PTR) impl_cast(OGLDrawImpl, draw_if, (PTR))


// mapping from GfxBlendMode to a pair of GfxBlendFactors.
static struct ogldraw_bm { GfxBlendFactor src, dst; } c_bmtobf[] = {
	{ gfxZero, gfxZero },                 // gfxBlendUnknown
	{ gfxOne, gfxZero },                  // gfxBlendCopy
	{ gfxSrcAlpha, gfxOneMinusSrcAlpha }, // gfxBlendNormal
	{ gfxOne, gfxOneMinusSrcAlpha },      // gfxBlendPremultiplied
	{ gfxSrcAlpha, gfxOne },              // gfxBlendAlphaAdd
	{ gfxSrcAlpha, gfxOneMinusSrcAlpha }, // gfxBlendModulate
	{ gfxOne, gfxOneMinusSrcAlpha },      // gfxBlendModulatePre
	{ gfxOne, gfxOne },                   // gfxBlendAdd
	{ 0, 0 },                             // gfxBlendSubtract
	{ gfxDstCol, gfxZero },               // gfxBlendMultiply
	{ gfxOne, gfxOneMinusSrcCol },        // gfxBlendScreen
};

static void ogldraw_setMode(OGLDrawImpl* self, GfxBlendMode mode)
{
	self->mode = mode; // avoid unnessary reconfigure.
	GfxRender_blendFunc(self->r, c_bmtobf[mode].src, c_bmtobf[mode].dst);
}


void ogldraw_save(GfxDraw ifptr)
{
	OGLDrawImpl* self = GfxDrawToGLImpl(ifptr);
	unsigned int newTop = self->stackTop + 1;
	// grow the stack if necessary.
	if (newTop == self->stackSize) {
		unsigned int newSize = self->stackSize + 8;
		OGLDrawState* newStack = (OGLDrawState*)
			realloc(self->stack, newSize * sizeof(OGLDrawState));
		if (!newStack) return; // bad luck.
		self->stack = newStack;
		self->stackSize = newSize;
	}
	// copy the active state to the next stack slot.
	// NB. self->ds is no longer valid if we grew the stack above.
	self->stack[newTop] = self->stack[self->stackTop];
	// make the copy active.
	self->ds = &self->stack[newTop];
	self->stackTop = newTop;
}

void ogldraw_restore(GfxDraw ifptr)
{
	OGLDrawImpl* self = GfxDrawToGLImpl(ifptr);
	if (self->stackTop > 0) { // never pop the first entry.
		// make the previous stack entry active.
		OGLDrawState* ds = &self->stack[--self->stackTop];
		self->ds = ds;
		// mark transform as dirty; (TODO) could check if transform has
		// changed since save() was called.
		self->transformDirty = true;
	}
}

void ogldraw_scale(GfxDraw ifptr, float sx, float sy)
{
	OGLDrawImpl* self = GfxDrawToGLImpl(ifptr);
	self->transformDirty = true;
	// TODO: untested, and not sure about pre/post.
	affine_pre_scale(&self->ds->transform, sx, sy, &self->ds->transform);
}

void ogldraw_rotate(GfxDraw ifptr, float angle)
{
	// TODO.
}

void ogldraw_translate(GfxDraw ifptr, float x, float y)
{
	OGLDrawImpl* self = GfxDrawToGLImpl(ifptr);
	self->transformDirty = true;
	affine_pre_translate(&self->ds->transform, x, y, &self->ds->transform);
}

void ogldraw_transform(GfxDraw ifptr, const Affine2D* transform)
{
	OGLDrawImpl* self = GfxDrawToGLImpl(ifptr);
	Affine2D temp = self->ds->transform;
	self->transformDirty = true;
	// TODO: not sure about left/right order here.
	affine_multiply(transform, &temp, &self->ds->transform);
}

void ogldraw_setTransform(GfxDraw ifptr, const Affine2D* transform)
{
	OGLDrawImpl* self = GfxDrawToGLImpl(ifptr);
	self->transformDirty = true;
	self->ds->transform = *transform;
}

void ogldraw_clearTransform(GfxDraw ifptr)
{
	OGLDrawImpl* self = GfxDrawToGLImpl(ifptr);
	self->transformDirty = true;
	self->ds->transform = c_affine2d_identity;
}

void ogldraw_blendMode(GfxDraw ifptr, GfxBlendMode mode, float alpha)
{
	OGLDrawImpl* self = GfxDrawToGLImpl(ifptr);
	OGLDrawState* ds = self->ds;
	// save new state values.
	ds->mode = mode;
	ds->alpha = alpha;
}

void ogldraw_lineStyle(GfxDraw ifptr, float lineWidth, GfxCapStyle cap, GfxJoinStyle join, float miterLimit)
{
	OGLDrawImpl* self = GfxDrawToGLImpl(ifptr);
	OGLDrawState* ds = self->ds;
	ds->lineCap = cap;
	ds->lineJoin = join;
	if (miterLimit > 0 /* not NaN */ && NOT_INF(miterLimit))
		ds->miterLimit = miterLimit;
}

void ogldraw_fillColor(GfxDraw ifptr, float red, float green, float blue, float alpha)
{
	OGLDrawImpl* self = GfxDrawToGLImpl(ifptr);
	OGLDrawState* ds = self->ds;
	ds->fillCol.r = red;
	ds->fillCol.g = green;
	ds->fillCol.b = blue;
	ds->fillCol.a = alpha;
}

void ogldraw_strokeColor(GfxDraw ifptr, float red, float green, float blue, float alpha)
{
	OGLDrawImpl* self = GfxDrawToGLImpl(ifptr);
	OGLDrawState* ds = self->ds;
	ds->strokeCol.r = red;
	ds->strokeCol.g = green;
	ds->strokeCol.b = blue;
	ds->strokeCol.a = alpha;
}

void ogldraw_clearRect(GfxDraw ifptr, float x, float y, float width, float height)
{
	// TODO.
}

void ogldraw_fillRect(GfxDraw ifptr, float x, float y, float width, float height)
{
	OGLDrawImpl* self = GfxDrawToGLImpl(ifptr);
	OGLDrawState* ds = self->ds;
	static float coords[8] = { 0, 0, 0, 1, 1, 1, 1, 0 };
	float verts[8] = { x, y, x, y + height, x + width, y + height, x + width, y };
	GfxRender_vertexCol(self->r, ds->fillCol.r, ds->fillCol.g, 
								 ds->fillCol.b, ds->fillCol.a * ds->alpha);
	if (self->mode != ds->mode)
		ogldraw_setMode(self, ds->mode);
	if (self->transformDirty) {
		self->transformDirty = false;
		GfxRender_transform(self->r, &self->ds->transform);
	}
	GfxRender_selectNone(self->r);
	GfxRender_quads2d(self->r, 1, verts, coords);
}

void ogldraw_strokeRect(GfxDraw ifptr, float x, float y, float width, float height)
{
	// TODO.
}

static void ogldraw_draw_img(OGLDrawImpl* self, GfxImage image,
							 const float* verts, const float* coords)
{
	// this is a bit of a hack really..
	if (self->ds->mode == gfxBlendModulate ||
		self->ds->mode == gfxBlendModulatePre)
		GfxRender_vertexCol(self->r, self->ds->fillCol.r,
			self->ds->fillCol.g, self->ds->fillCol.b, self->ds->fillCol.a);
	else
		GfxRender_vertexCol(self->r, 1, 1, 1, self->ds->alpha);
	if (self->mode != self->ds->mode)
		ogldraw_setMode(self, self->ds->mode);
	if (self->transformDirty) {
		self->transformDirty = false;
		GfxRender_transform(self->r, &self->ds->transform);
	}
	GfxRender_select(self->r, image);
	GfxRender_quads2d(self->r, 1, verts, coords);
}

static const float c_quad_coords[8] = { 0, 0, 0, 1, 1, 1, 1, 0 };

void ogldraw_drawImage(GfxDraw ifptr, GfxImage image, float dx, float dy)
{
	OGLDrawImpl* self = GfxDrawToGLImpl(ifptr);
	OGLImageImpl* img = GfxImageToImpl(image);
	float dw = (float)img->width, dh = (float)img->height;
	float verts[8] = { dx, dy, dx, dy + dh, dx + dw, dy + dh, dx + dw, dy };
	ogldraw_draw_img(self, image, verts, c_quad_coords);
}

void ogldraw_drawImageRect(GfxDraw ifptr, GfxImage image,
						   float dx, float dy, float dw, float dh)
{
	OGLDrawImpl* self = GfxDrawToGLImpl(ifptr);
	float verts[8] = { dx, dy, dx, dy + dh, dx + dw, dy + dh, dx + dw, dy };
	ogldraw_draw_img(self, image, verts, c_quad_coords);
}

void ogldraw_drawImageSubRect(GfxDraw ifptr, GfxImage image,
							  float sx, float sy, float sw, float sh,
							  float dx, float dy, float dw, float dh)
{
	OGLDrawImpl* self = GfxDrawToGLImpl(ifptr);
	OGLImageImpl* img = GfxImageToImpl(image);
	float oow = 1.0f / (float)img->width, ooh = 1.0f / (float)img->height;
	float left = sx * oow, top = sy * ooh;
	float right = left + (sw * oow), bottom = top + (sh * ooh);
	float coords[8] = { left, top, left, bottom, right, bottom, right, top };
	float verts[8] = { dx, dy, dx, dy + dh, dx + dw, dy + dh, dx + dw, dy };
	ogldraw_draw_img(self, image, verts, c_quad_coords);
}

void ogldraw_copyImageRect(GfxDraw ifptr, GfxImage destImage, int dx, int dy, int dw, int dh, int sx, int sy)
{
	OGLDrawImpl* self = GfxDrawToGLImpl(ifptr);
	OGLRenderImpl* rend = GfxRenderToImpl(self->r);
	OGLImageImpl* img = GfxImageToImpl(destImage);
	// bind the destination texture (maintaining render invariants)
	ogl_r_select(self->r, destImage);
	// make sure dest rect is within the texture.
	if (dx < 0) { sx += -dx; dx = 0; }
	if (dy < 0) { sy += -dy; dy = 0; }
	if (dx + dw > img->width) dw = img->width - dx;
	if (dy + dh > img->height) dh = img->height - dy;
	// read the pixels.
	glCopyTexSubImage2D(GL_TEXTURE_2D, 0, dx, /*img->height -*/ dy, sx, rend->height - sy, dw, dh);
}

void ogldraw_pushLayer(GfxDraw ifptr)
{
	// TODO.
}

void ogldraw_popLayer(GfxDraw ifptr, GfxBlendMode mode, float alpha)
{
	// TODO.
}

void ogldraw_begin(GfxDraw ifptr)
{
	OGLDrawImpl* self = GfxDrawToGLImpl(ifptr);
	iPair size = GfxContext_getTargetSize(self->cx);

	// set up an orthographic projection for 2D rendering.
	// TODO: could ask the context if projection has changed.
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, size.x, size.y, 0, 1, -1);
	glMatrixMode(GL_MODELVIEW);

	// don't need perspecive correction for 2D.
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);

	// don't care which way triangles are wound.
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glDisable(GL_CULL_FACE);

	// this is the default?
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	// any outside drawing means we've lost pipeline state.
	self->mode = gfxBlendUnknown;
	self->transformDirty = true;
}

void ogldraw_end(GfxDraw ifptr)
{
}

void ogldraw_clear(GfxDraw ifptr, GfxCol col)
{
	glClearColor(col.r, col.g, col.b, col.a);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
}

GfxImage ogldraw_createImage(GfxDraw ifptr)
{
	OGLDrawImpl* self = GfxDrawToGLImpl(ifptr);
	return GfxContext_createImage(self->cx);
}

static void ogldraw_setRenderTarget(GfxDraw ifptr, GfxImage target)
{
	OGLDrawImpl* self = GfxDrawToGLImpl(ifptr);
	iPair size;

	GfxRender_setRenderTarget(self->r, target);

	// set up an orthographic projection for 2D rendering.
	size = GfxContext_getTargetSize(self->cx);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	// left, right, bottom, top
	// since we flip all images on output, we need to invert the
	// coordinate system again when rendering to a texture.
	if (target)
		glOrtho(0, size.x, 0, size.y, 1, -1);
	else
		glOrtho(0, size.x, size.y, 0, 1, -1);
	glMatrixMode(GL_MODELVIEW);
}

static void ogldraw_clearRenderTarget(GfxDraw ifptr)
{
	ogldraw_setRenderTarget(ifptr, 0);
}

static GfxDraw_i ogldraw_if = {
	ogldraw_save,
	ogldraw_restore,
	ogldraw_scale,
	ogldraw_rotate,
	ogldraw_translate,
	ogldraw_transform,
	ogldraw_setTransform,
	ogldraw_clearTransform,
	ogldraw_blendMode,
	ogldraw_lineStyle,
	ogldraw_fillColor,
	ogldraw_strokeColor,
	ogldraw_clearRect,
	ogldraw_fillRect,
	ogldraw_strokeRect,
	ogldraw_drawImage,
	ogldraw_drawImageRect,
	ogldraw_drawImageSubRect,
	ogldraw_copyImageRect,
	ogldraw_pushLayer,
	ogldraw_popLayer,
	ogldraw_begin,
	ogldraw_end,
	ogldraw_clear,
	ogldraw_createImage,
	ogldraw_setRenderTarget,
	ogldraw_clearRenderTarget,
};

static const GfxCol c_gfxcol_black = {0,0,0,1};
static const fRect c_empty_rect = {0,0,0,0};

GfxDraw createGfxDraw(GfxContext context)
{
	OGLDrawImpl* self = cpart_new(OGLDrawImpl);
	self->draw_if = &ogldraw_if;
	self->cx = context;
	self->r = GfxContext_getRenderer(context);
	self->mode = gfxBlendCopy; // blending initially disabled.
	self->transformDirty = true; // need to set render state.
	self->stack = (OGLDrawState*)cpart_alloc(8 * sizeof(OGLDrawState));
	self->ds = self->stack;
	self->stackTop = 0;
	self->stackSize = 8;
	affine_set_identity(&self->ds->transform);
	self->ds->clip = c_empty_rect;
	self->ds->fillCol = c_gfxcol_black;
	self->ds->strokeCol = c_gfxcol_black;
	self->ds->alpha = 1.0f;
	self->ds->lineWidth = 1.0f;
	self->ds->miterLimit = 10.0f;
	self->ds->lineCap = gfxCapButt;
	self->ds->lineJoin = gfxJoinMiter;
	self->ds->mode = gfxBlendNormal;
	return &self->draw_if;
}
