#ifndef CPART_DRAW
#define CPART_DRAW

#include "graphics.h"

// interface declarations:
typedef struct GfxDraw_i GfxDraw_i, **GfxDraw;

typedef enum GfxBlendMode {
	gfxBlendUnknown = 0, // blending state is unknown.
	gfxBlendCopy,       // (GL_ONE, GL_ZERO)
	gfxBlendNormal,     // (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)
	gfxBlendPremultiplied,// (GL_SRC_ALPHA, GL_ONE)
	gfxBlendAlphaAdd,	// (GL_SRC_ALPHA, GL_ONE)
	gfxBlendModulate,   // gfxBlendNormal modulated by fillColor.
	gfxBlendModulatePre,// gfxBlendPremultiplied modulated by fillColor.
	gfxBlendAdd,        // (GL_ONE, GL_ONE)
	gfxBlendSubtract,   // A + B - 1
	gfxBlendMultiply,   // (GL_DST_COLOR, GL_ZERO)
	gfxBlendScreen,     // 1-(1-A)*(1-B) or A+(B*(1-A)) or (ONE, ONE_MINUS_SRC_COLOR)
	gfxBlendDarken,     // min(A,B)
	gfxBlendLighten,    // max(A,B)
	gfxBlendDifference, // abs(A-B) or sat(A-B) + sat(B-A)
	gfxBlendExclusion,  // A+B-2AB
	gfxBlendOverlay,    // 2AB (A < 0.5) 1-2*(1-A)*(1-B)
	gfxBlendHardLight,  // 2AB (B < 0.5) 1-2*(1-A)*(1-B)
	gfxBlendSoftLight,  // 2*A*B+A2*(1-2*B) (B < 0.5) sqrt(A)*(2*b-1)+(2*a)*(1-b)
	gfxBlendDodge,      // A / (1-B)
	gfxBlendBurn,       // 1 - (1-A) / B
	_gfxBlendModeSize=0x7f
} GfxBlendMode;         // NB. A=dst, B=src !

typedef enum GfxCapStyle {
	gfxCapButt = 0,
	gfxCapRound,
	gfxCapSquare,
} GfxCapStyle;

typedef enum GfxJoinStyle {
	gfxJoinMiter = 0,
	gfxJoinRound,
	gfxJoinBevel,
} GfxJoinStyle;


// GfxDraw interface.

// A high level drawing interface for 2D rendering.

// HTML5 2d canvas:
// save, restore (all state)
// scale, rotate, translate, transform(2x3), setTransform(2x3)
// globalAlpha, globalCompositeOp
// strokeStyle, fillStyle (colour, gradient brush or pattern brush)
// lineWidth, lineCap, lineJoin, miterLimit
// clearRect, fillRect, strokeRect
// beginPath, closePath, moveTo, lineTo, quadraticCurveTo, bezierCurveTo,
//   arcTo, rect, arc, fill, stroke, clip, isPointInPath
// font, textAlign, textBaseline
// fillText, strokeText, measureText
// drawImage(x,y,w,h), drawImage(srcRect,dstRect)

struct GfxDraw_i {
	// push current drawing state to the state stack.
	void (*save)(GfxDraw self);
	// restore drawing state from the state stack.
	void (*restore)(GfxDraw self);
	// apply a scale in addition to the current transform.
	void (*scale)(GfxDraw self, float sx, float sy);
	// apply a rotation in addition to the current transform.
	void (*rotate)(GfxDraw self, float angle);
	// apply a translation in addition to the current transform.
	void (*translate)(GfxDraw self, float x, float y);
	// apply a transform in addition to the current transform.
	void (*transform)(GfxDraw self, const Affine2D* transform);
	// set the affine transform for subsequent rendering.
	void (*setTransform)(GfxDraw self, const Affine2D* transform);
	// reset the affine transform to the identity transform.
	void (*clearTransform)(GfxDraw self);
	// set up the specified blending mode.
	void (*blendMode)(GfxDraw self, GfxBlendMode mode, float alpha);
	// set stroke thickness and line cap and join modes.
	void (*lineStyle)(GfxDraw self, float lineWidth, GfxCapStyle cap, GfxJoinStyle join, float miterLimit);
	// set a solid colour fill brush.
	void (*fillColor)(GfxDraw self, float red, float green, float blue, float alpha);
	// set a solid colour stroke brush.
	void (*strokeColor)(GfxDraw self, float red, float green, float blue, float alpha);
	// clear a rectangle to the transparent colour.
	void (*clearRect)(GfxDraw self, float x, float y, float width, float height);
	// fill a rectangle with the fill brush.
	void (*fillRect)(GfxDraw self, float x, float y, float width, float height);
	// stroke the outline of a rect with the stroke brush.
	void (*strokeRect)(GfxDraw self, float x, float y, float width, float height);
	// render an image to the specified rectangle.
	void (*drawImage)(GfxDraw self, GfxImage image, float dx, float dy);
	// render an image to the specified rectangle.
	void (*drawImageRect)(GfxDraw self, GfxImage image, float dx, float dy, float dw, float dh);
	// render an image sub-rect to the specified rectangle.
	void (*drawImageSubRect)(GfxDraw self, GfxImage image, float sx, float sy, float sw, float sh, float dx, float dy, float dw, float dh);
	// copy pixels from the current render target to an image.
	void (*copyImageRect)(GfxDraw self, GfxImage destImage, int dx, int dy, int dw, int dh, int sx, int sy);
	// begin drawing on a new layer until popLayer is called.
	// the current state will be saved; it will be restored by popLayer.
	void (*pushLayer)(GfxDraw self);
	// merge the active layer down over the previously active layer.
	void (*popLayer)(GfxDraw self, GfxBlendMode mode, float alpha);
	// begin 2D drawing in the host GfxContext.
	void (*begin)(GfxDraw self);
	// end 2D drawing in the host GfxContext.
	void (*end)(GfxDraw self);
	// clear the entire canvas.
	void (*clear)(GfxDraw self, GfxCol color);
	// create an image object.
	GfxImage (*createImage)(GfxDraw self);
	// set the current rendering target to an image.
	// for best performance the image should be created with the
	// gfxImgRenderTarget flag; otherwise extra copying may be performed.
	void (*setRenderTarget)(GfxDraw self, GfxImage target);
	// restore the system framebuffer target.
	void (*clearRenderTarget)(GfxDraw self);
};

#define GfxDraw_save(self) (*(self))->save((self))
#define GfxDraw_restore(self) (*(self))->restore((self))
#define GfxDraw_scale(self,sx,sy) (*(self))->scale((self),(sx),(sy))
#define GfxDraw_rotate(self,angle) (*(self))->rotate((self),(angle))
#define GfxDraw_translate(self,x,y) (*(self))->translate((self),(x),(y))
#define GfxDraw_transform(self,affine) (*(self))->transform((self),(affine))
#define GfxDraw_setTransform(self,affine) (*(self))->setTransform((self),(affine))
#define GfxDraw_clearTransform(self) (*(self))->clearTransform((self))
#define GfxDraw_blendMode(self,mode,alpha) (*(self))->blendMode((self),(mode),(alpha))
#define GfxDraw_lineStyle(self,lineWidth,cap,join,miterLimit) (*(self))->lineStyle((self),(lineWidth),(cap)),(join),(miterLimit))
#define GfxDraw_fillColor(self,red,green,blue,alpha) (*(self))->fillColor((self),(red),(green),(blue),(alpha))
#define GfxDraw_strokeColor(self,col) (*(self))->strokeColor((self),(red),(green),(blue),(alpha))
#define GfxDraw_clearRect(self,x,y,width,height) (*(self))->clearRect((self),(x),(y),(width),(height))
#define GfxDraw_fillRect(self,x,y,width,height) (*(self))->fillRect((self),(x),(y),(width),(height))
#define GfxDraw_strokeRect(self,x,y,width,height) (*(self))->strokeRect((self),(x),(y),(width),(height))
#define GfxDraw_drawImage(self,image,dx,dy) (*(self))->drawImage((self),(image),(dx),(dy))
#define GfxDraw_drawImageRect(self,image,dx,dy,dw,dh) (*(self))->drawImageRect((self),(image),(dx),(dy),(dw),(dh))
#define GfxDraw_drawImageSubRect(self,image,sx,sy,sw,sh,dx,dy,dw,dh) (*(self))->drawImageSubRect((self),(image),(sx),(sy),(sw),(sh),(dx),(dy),(dw),(dh))
#define GfxDraw_copyImageRect(self,destImage,dx,dy,dw,dh,sx,sy) (*(self))->copyImageRect((self),(destImage),(dx),(dy),(dw),(dh),(sx),(sy))
#define GfxDraw_pushLayer(self) (*(self))->pushLayer((self))
#define GfxDraw_popLayer(self,mode,alpha) (*(self))->popLayer((self),(mode),(alpha))
#define GfxDraw_begin(self) (*(self))->begin((self))
#define GfxDraw_end(self) (*(self))->end((self))
#define GfxDraw_clear(self,color) (*(self))->clear((self),(color))
#define GfxDraw_createImage(self) (*(self))->createImage((self))
#define GfxDraw_setRenderTarget(self,target) (*(self))->setRenderTarget((self),(target))
#define GfxDraw_clearRenderTarget(self) (*(self))->clearRenderTarget((self))


// GfxDraw factory.

GfxDraw createGfxDraw(GfxContext context);


#endif
