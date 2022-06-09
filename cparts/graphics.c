import * from affine

struct SurfaceData;

// interface declarations:
typedef struct GfxContext_i GfxContext_i, **GfxContext;
typedef struct GfxScene_i GfxScene_i, **GfxScene;
typedef struct GfxRender_i GfxRender_i, **GfxRender;
typedef struct GfxImage_i GfxImage_i, **GfxImage;
typedef struct GfxShader_i GfxShader_i, **GfxShader;
typedef struct GfxProgram_i GfxProgram_i, **GfxProgram;
typedef struct GfxTarget_i GfxTarget_i, **GfxTarget;
typedef struct GfxBackend_i GfxBackend_i, **GfxBackend;

typedef struct GfxCol { float r, g, b, a; } GfxCol;


// GfxImage interface.

enum GfxImageFormat {
	gfxFormatA8 = 1,
	gfxFormatRGB8 = 3,
	gfxFormatRGBA8 = 4,
}

enum GfxImageFlags {
	// The image is continuous in X (tiles left-to-right)
	// Non-power-of-two images will be scaled up if necessary.
	gfxImgWrapX			= 1,
	// The image is continuous in Y (tiles top-to-bottom)
	// Non-power-of-two images will be scaled up if necessary.
	gfxImgWrapY			= 2,
	// The image is continuous in X and Y (tiles in both directions)
	// Non-power-of-two images will be scaled up if necessary.
	gfxImgWrap			= gfxImgWrapX | gfxImgWrapY,
	// Generate MipMaps to reduce aliasing artefacts when scaling.
	gfxImgMipMap		= 4,
	// Compress images using lossy compression, if supported.
	gfxImgCompress		= 8,
	// Use nearest-neighbour magnification instead of smoothing.
	gfxImageMagNearest	= 16,
	// Use nearest-neighbour minification instead of smoothing.
	gfxImageMinNearest	= 32,
	// Do not fill the image; just allocate memory.
	gfxImageDoNotFill	= 64,
	// Use an internal format compatible with render-to-texture.
	gfxImageRenderTarget = 128,
	// internal: mask unused flags bits.
	gfx_imageFlagUnused	= 256,
}

interface GfxImage {
	// these private fields implement reference counting.
	// use retain() and release() global functions.
	ptrdiff_t _ref_disp;
	void (*_destruct)(GfxImage self);
	// upload image data to an image object.
	void (*upload)(GfxImage self, SurfaceData sd, GfxImageFlags flags);
	// create an image filled with a solid colour.
	bool (*create)(GfxImage self, GfxImageFormat format,
				   unsigned int width, unsigned int height,
				   RGBA col, int /* GfxImageFlags */ flags);
	// upload image data to a sub-rect of an image object.
	// NB. the coordinates (x,y) are relative to the bottom-left corner.
	void (*update)(GfxImage self, int x, int y, SurfaceData sd);
	// retrieve the allocated image dimensions.
	iPair (*getSize)(GfxImage self);
	// read back the image data.
	void (*read)(GfxImage self, SurfaceData sd);
}

/*
	// allocate an image of the requested size and usage.
	bool (*allocate)(GfxImage self, GfxImageFormat format,
		unsigned int width, unsigned int height, unsigned int levels,
		GfxImageFlags flags);
	// clear the image to the specified colour.
	void (*clear)(GfxImage self, const GfxCol* colour);
	// upload image data.
	void (*upload)(GfxImage self, unsigned int level, struct SurfaceData* sd);
	// upload a sub-rect of image data.
	void (*update)(GfxImage self, unsigned int level,
		unsigned int x, unsigned int y, struct SurfaceData* sd);
*/

#define GfxImage_upload(self,surfaceData,flags) (*(self))->upload((self),(surfaceData),(flags))
#define GfxImage_create(self,format,width,height,col,flags) (*(self))->create((self),(format),(width),(height),(col),(flags))
#define GfxImage_update(self,x,y,surfaceData) (*(self))->update((self),(x),(y),(surfaceData))
#define GfxImage_getSize(self) (*(self))->getSize((self))
#define GfxImage_read(self,sd) (*(self))->read((self),(sd))


// GfxShader interface.

// A shader is an instance of a GfxProgram with argument bindings.

enum GfxBlendFactor {
	gfxZero = 0,
	gfxOne = 1,
	gfxSrcCol,
	gfxOneMinusSrcCol,
	gfxSrcAlpha,
	gfxOneMinusSrcAlpha,
	gfxDstAlpha,
	gfxOneMinusDstAlpha,
	gfxDstCol,
	gfxOneMinusDstCol,
}

interface GfxShader {
	// these private fields implement reference counting.
	// use retain() and release() global functions.
	ptrdiff_t _ref_disp;
	void (*_destruct)(GfxShader self);

	// set the target surface blending function.
	void (*setBlendFunc)(GfxShader self, GfxBlendFactor srcFactor, GfxBlendFactor dstFactor);

	// bind an image object to an image parameter.
	void (*bindImage)(GfxShader self, unsigned int param, GfxImage image);
}

#define GfxShader_setBlendFunc(self,srcFactor,dstFactor) (*(self))->setBlendFunc((self),(srcFactor),(dstFactor))
#define GfxShader_bindImage(self,param,image) (*(self))->bindImage((self),(param),(image))


// GfxTarget interface.

enum GfxTargetFlags {
	gfxTargetHasColour			= 1,  // allocate colour buffer.
	gfxTargetHasDepth			= 2,  // allocate depth buffer.
	gfxTargetHasStencil			= 4,  // allocate stencil buffer.
	gfxTargetKeepColour			= 8,  // render to colour texture.
	gfxTargetKeepDepth			= 16, // render to depth texture.
	gfxTargetKeepStencil		= 32, // render to stencil texture.
	gfxTargetKeepDepthStencil	= 64, // render to packed depth-stencil texture.
	gfx_bufferFlagUnused		= 128,
}

typedef void (*GfxRenderFunc)(void* data, GfxRender render);

interface GfxTarget {
	// these private fields implement reference counting.
	// use retain() and release() global functions.
	ptrdiff_t _ref_disp;
	void (*_destruct)(GfxTarget self);
	// begin rendering into this render target.
	GfxRender (*begin)(GfxTarget self);
	// end rendering into this target but do not commit;
	// call begin() again to resume rendering.
	void (*end)(GfxTarget self);
	// end rendering and commit the rendered scene to an image,
	// which will remain valid until the next call on this target.
	GfxImage (*commit)(GfxTarget self);
	// resize the target surface area.
	bool (*resize)(GfxTarget self, int width, int height);
}

#define GfxTarget_begin(SELF) (*(SELF))->begin((SELF))
#define GfxTarget_end(SELF) (*(SELF))->end((SELF))
#define GfxTarget_commit(SELF) (*(SELF))->commit((SELF))
#define GfxTarget_resize(SELF,WIDTH,HEIGHT) (*(SELF))->resize((SELF),(WIDTH),(HEIGHT))


// GfxRender interface.

// (a) bind all args to the context
// (b) bind some args to shader, some to context
// (c) bind all args to the shader

// NB. binding everything to the shader has the benefit that shaders
// can be cascaded or nested while retaining their argument bindings,
// and the backend can coalesce such composite shaders.

// Note that there is a cost benefit trade-off here; how much work
// should the backend commit to coalescing or optimising bindings
// that will only be used once? This can be specified at bind time.

// OTOH, binding to the context allows bindings to be re-used across
// shaders, but then they must be unbound again.

// Immediate arguments avoid the binding and unbinding costs for
// single-use arguments; identify the arguments that are commonly
// used once.

// Index buffers (typically one)
// Vertex buffers (input streams)
// Image bindings (source textures)
// Transforms (program constants or locals)
// Literals (program constants or locals)
// Query buffers (input stream from scene query, e.g. lights)

// A static vertex buffer can be "optimised" (interleaved and transcoded)
// for a shader, but the original streams will need to be retained in
// case the buffer is used with other shaders.

// An abstraction over this: buffers can be placed into buffer sets
// and optimised for some set of usages, where usages are derived from
// the shader techniques that will be used with the buffers.
// The explicit optimise request takes a set of heterogeneous tagged
// buffers and produces a new set of heterogeneous tagged buffers.
// Usages can be compared for equality, subsetting, difference in size
// due to elements not in common to both usages.

// Buffer sets can be optimised while empty, then subsequently filled
// with data by the application; the application will need to query
// the data format and stride assigned to each stream (if any) and
// request to lock each of the new buffers.

enum GfxVertexFormats { // no, this is a bad idea.
	gfxVertX   = 0x0000,
	gfxVertXY  = 0x0001,
	gfxVertXYZ = 0x0002,
	gfxVertXYZW= 0x0003,
	gfxVertU   = 0x0004,
	gfxVertUV  = 0x0005,
	gfxVertUVS = 0x0006,
	gfxVertUVST= 0x0007,
}

enum GfxIndexFormats {
	gfxIndex8  = 0,
	gfxIndex16 = 1,
	gfxIndex32 = 2,
}

interface GfxRender {
	void (*select)(GfxRender self, GfxImage image);
	void (*selectNone)(GfxRender self);
	void (*quads2d)(GfxRender self, int num, const float* verts, const float* coords);
	void (*vertexCol)(GfxRender self, float red, float green, float blue, float alpha);
	void (*blendFunc)(GfxRender self, GfxBlendFactor src, GfxBlendFactor dest);
	void (*transform)(GfxRender self, const Affine2D* transform);
	void (*clearTransform)(GfxRender self);
	void (*clear)(GfxRender self, const GfxCol* col);
	/*
	void (*setShader)(GfxRender self, GfxShader shader);
	void (*drawQuadsPtr)(GfxRender self, int numQuads, int vertexFormat, const void* vertexData);
	void (*drawQuadsIndPtr)(GfxRender self, int numQuads, int vertexFormat, const void* vertexData, int indexFormat, const void* indexData);
	void (*drawTrisIndPtr)(GfxRender self, int numTris, int vertexFormat, const void* vertexData, int indexFormat, const void* indexData);
	void (*drawStripIndPtr)(GfxRender self, int numTris, int vertexFormat, const void* vertexData, int indexFormat, const void* indexData);
	void (*copyToImage)(GfxRender self, const iRect* srcRect, GfxImage destImage, const iRect* destRect);
	*/
	// set the current rendering target.
	// pass target=0 to select the system framebuffer.
	// for best performance the image should be created with the
	// gfxImgRenderTarget flag, otherwise extra copying may be performed.
	void (*setRenderTarget)(GfxRender self, GfxImage target);
}

#define GfxRender_select(SELF,IMG) (*(SELF))->select((SELF),(IMG))
#define GfxRender_selectNone(SELF) (*(SELF))->selectNone((SELF))
#define GfxRender_quads2d(SELF,NUM,VERTS,COORDS) (*(SELF))->quads2d((SELF),(NUM),(VERTS),(COORDS))
#define GfxRender_vertexCol(SELF,R,G,B,A) (*(SELF))->vertexCol((SELF),(R),(G),(B),(A))
#define GfxRender_blendFunc(SELF,SRC,DST) (*(SELF))->blendFunc((SELF),(SRC),(DST))
#define GfxRender_transform(SELF,TX) (*(SELF))->transform((SELF),(TX))
#define GfxRender_clearTransform(SELF) (*(SELF))->clearTransform((SELF))
#define GfxRender_clear(self,col) (*(self))->clear((self),(col))
#define GfxRender_setRenderTarget(self,target) (*(self))->setRenderTarget((self),(target))

/*
#define GfxRender_setShader(self,shader) (*(self))->setShader((self),(shader))
#define GfxRender_drawQuadsPtr(self,numQuads,vertexFormat,vertexData) (*(self))->drawQuadsPtr((self),(numQuads),(vertexFormat),(vertexData))
#define GfxRender_drawQuadsIndPtr(self,numQuads,vertexFormat,vertexData,indexFormat,indexData) (*(self))->drawQuadsIndPtr((self),(numQuads),(vertexFormat),(vertexData),(indexFormat),(indexData))
#define GfxRender_copyToImage(self,srcRect,destImage,destRect) (*(self))->copyToImage((self),(srcRect),(destImage),(destRect))
*/


// GfxScene interface.

// A scene renderable to display in a GfxContext.

typedef long GfxDelta;

interface GfxScene {
	void (*init)(GfxScene self, GfxContext context);
	void (*final)(GfxScene self);
	void (*sized)(GfxScene self, int width, int height);
	void (*render)(GfxScene self, GfxRender gfx);
	void (*update)(GfxScene self, GfxDelta delta);
}


// GfxScene factories.

// Render a two-dimensional scene with the origin in the top-left
// corner (+X right, +Y down) and pixel unit coordinates.
GfxScene createGfxOrtho2D(GfxScene scene);


// GfxBackend interface.

// Implemented by the graphics library backend for front-end views.

interface GfxBackend {
	void (*init)(GfxBackend self);
	void (*final)(GfxBackend self);
	void (*sized)(GfxBackend self, int width, int height);
	void (*render)(GfxBackend self);
	void (*update)(GfxBackend self, GfxDelta delta);
	void (*setScene)(GfxBackend self, GfxScene scene);
	GfxContext (*getContext)(GfxBackend self); // sigh.
}

GfxBackend createGfxOpenGLBackend();


// GfxContext interface.

// A rendering context attached to a GfxBackend.

interface GfxContext {
/*
	// these private fields implement reference counting.
	// use retain() and release() global functions.
	ptrdiff_t _ref_disp;
	void (*_destruct)(GfxContext self);

	// clear the surfaces bound to this context.
	void (*clear)(GfxContext self, const GfxCol* col);

	// begin rendering into this render target.
	GfxRender (*begin)(GfxTarget self);

	// end rendering into this target but do not commit;
	// call begin() again to resume rendering.
	void (*end)(GfxTarget self);

	// end rendering and commit the rendered scene to an image,
	// which will remain valid until the next call on this target.
	GfxImage (*commit)(GfxTarget self);

	// resize the target surface area.
	bool (*resize)(GfxTarget self, int width, int height);
*/
	// create an image.
	GfxImage (*createImage)(GfxContext self /*GfxImageFormat format,
		unsigned int width, unsigned int height, unsigned int levels,
		GfxImageFlags flags, GfxImageFormat dataFormat,
		unsigned int stride, byte* data */);

	// create an offscreen rendering target.
	// ->clone(share flags)
	GfxTarget (*createTarget)(GfxContext self, GfxImageFormat format,
		unsigned int width, unsigned int height, unsigned int samples,
		GfxTargetFlags flags);

	// create a shading program object.
	GfxShader (*createShader)(GfxContext self);

	// get a rendering interface, oh no a getter.
	GfxRender (*getRenderer)(GfxContext self);

	// get the size of the active render target.
	iPair (*getTargetSize)(GfxContext self);

/*
	// create a vertex buffer.
	GfxTarget (*createVertexBuffer)(GfxContext self, size_t size,
		GfxVertexFormat format, GfxTargetFlags flags);

	// create an index buffer.
	GfxTarget (*createIndexBuffer)(GfxContext self, size_t size,
		GfxVertexFormat format, GfxTargetFlags flags);
*/
}

#define GfxContext_createImage(SELF) (*(SELF))->createImage((SELF))
#define GfxContext_createTarget(SELF,FORMAT,WIDTH,HEIGHT,SAMPLES,FLAGS) (*(SELF))->createTarget((SELF),(FORMAT),(WIDTH),(HEIGHT),(SAMPLES),(FLAGS))
#define GfxContext_createShader(self) (*(self))->createShader((self))
#define GfxContext_getRenderer(self) (*(self))->getRenderer((self))
#define GfxContext_getTargetSize(self) (*(self))->getTargetSize((self))
