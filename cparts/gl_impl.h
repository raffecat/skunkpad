#ifndef CPART_OGL_IMPL
#define CPART_OGL_IMPL

/* OpenGL headers */

#ifdef _WIN32
#include <windows.h>
#else
#include <X11/Xlib.h>
#endif

#include <GL/gl.h>
#pragma comment(lib, "opengl32.lib")

#include <GL/glu.h>
#pragma comment(lib, "glu32.lib")

#include "gl/glext.h"

#ifdef _WIN32
#include "gl/wglext.h"
#endif

#ifndef GL_CLAMP_TO_EDGE /* sigh */
//#define GL_CLAMP_TO_EDGE 0x812F
#endif

#include "graphics.h"

typedef struct OGLImageImpl OGLImageImpl;
typedef struct OGLTargetImpl OGLTargetImpl;
typedef struct OGLShaderImpl OGLShaderImpl;
typedef struct OGLRenderImpl OGLRenderImpl;
typedef struct OGLContextImpl OGLContextImpl;


// OpenGL extensions.

bool ogl_has_extension(const char* name);
bool ogl_has_wgl_extension(const char* name);
bool ogl_has_ext_extension(const char* name);

#ifdef _WIN32
#define ogl_get_proc wglGetProcAddress
#endif

void ogl_init_framebuffer_object();
void ogl_init_render_texture();


// GfxImage private interface.
/*
typedef struct GfxImageImpl_i {
	GfxImage_i image; // embed the public interface.
	void (*bind)(OGLImageImpl* self);
} GfxImageImpl_i;
*/


// GfxRender implementation.

enum gfx_ogl_r_flags {
	//ogl_en_tex2d = 1,
	ogl_en_vert_a = 2,
	ogl_en_tex0_a = 4,
};

struct OGLRenderImpl {
	GfxRender_i* render_i;		// provides GfxRender for downstream.
	int enabled;				// active glEnable flags.
	GfxImage selectedImg;		// currently selected image.
	GfxImage target;			// current render target.
	GLuint fbo;					// FBO for render target.
	int width, height;			// size of the render target.
	OGLContextImpl* cx;			// owner GfxContext.
};

#define GfxRenderToImpl(PTR) ((OGLRenderImpl*)(PTR))

void gfx_ogl_render_init(OGLRenderImpl* self, OGLContextImpl* cx);
void ogl_r_select(GfxRender ifptr, GfxImage image);


// GfxTarget implementation.

struct OGLTargetImpl {
	GfxTarget_i* target_i;
	OGLContextImpl* cx;			// owner or subordinate GfxContext.
	size_t refs;
	int flags;
	int width, height;
	HPBUFFERARB pbuf;
	HDC dc, prev_dc;
	HGLRC rc, prev_rc;
	GLuint fbo, colBuf, depthBuf;
	GfxImage colImg, depthImg;
	OGLTargetImpl* next;
};

#define GfxTargetToImpl(PTR) impl_cast(OGLTargetImpl, target_i, (PTR))

OGLTargetImpl* gfx_ogl_target_new(OGLContextImpl* context, int width, int height, GfxTargetFlags flags);


// GfxImage implementation.

struct OGLImageImpl {
	GfxImage_i* image_i;
	size_t refs;
	int flags;
	int width, height;
	GLuint id;
	OGLImageImpl* next;
};

#define GfxImageToImpl(PTR) impl_cast(OGLImageImpl, image_i, (PTR))

typedef enum OGLImageImplFlags {
	gfxImageFlagIsPotX		= gfx_imageFlagUnused,
	gfxImageFlagIsPotY		= gfx_imageFlagUnused*2,
	gfxImageFlagIsPotBoth   = gfxImageFlagIsPotX | gfxImageFlagIsPotY,
} OGLImageImplFlags;


// GfxContext implementation.

struct OGLContextImpl {
	OGLRenderImpl render;		// must be first member.
	GfxContext_i* context_i;	// provides GfxContext for downstream.
	GfxBackend_i* backend_i;	// provides GfxBackend for upstream.
	GfxScene scene;				// downstream scene.
	OGLImageImpl* images;		// linked list of all images.
	OGLTargetImpl* targets;		// linked list of all target.
	OGLShaderImpl* shaders;		// linked list of all shaders.
	int width, height;			// size of the system framebuffer.
};

#define GfxContextToImpl(PTR) impl_cast(OGLContextImpl, context_i, (PTR))
#define GfxBackendToImpl(PTR) impl_cast(OGLContextImpl, backend_i, (PTR))


// Extensions.

// GL_ARB_framebuffer_object / EXT_framebuffer_object
extern bool ogl_framebuffer_object;
extern bool ogl_framebuffer_blit;
extern bool ogl_framebuffer_multisample;
extern bool ogl_packed_depth_stencil;
extern bool ogl_framebuffer_is_arb;
extern PFNGLBINDRENDERBUFFERPROC oglBindRenderbuffer;
extern PFNGLDELETERENDERBUFFERSPROC oglDeleteRenderbuffers;
extern PFNGLGENRENDERBUFFERSPROC oglGenRenderbuffers;
extern PFNGLRENDERBUFFERSTORAGEPROC oglRenderbufferStorage;
extern PFNGLGETRENDERBUFFERPARAMETERIVPROC oglGetRenderbufferParameteriv;
extern PFNGLBINDFRAMEBUFFERPROC oglBindFramebuffer;
extern PFNGLDELETEFRAMEBUFFERSPROC oglDeleteFramebuffers;
extern PFNGLGENFRAMEBUFFERSPROC oglGenFramebuffers;
extern PFNGLCHECKFRAMEBUFFERSTATUSPROC oglCheckFramebufferStatus;
extern PFNGLFRAMEBUFFERTEXTURE2DPROC oglFramebufferTexture2D;
extern PFNGLFRAMEBUFFERRENDERBUFFERPROC oglFramebufferRenderbuffer;
extern PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC oglGetFramebufferAttachmentParameteriv;
extern PFNGLGENERATEMIPMAPPROC oglGenerateMipmap;
extern PFNGLBLITFRAMEBUFFERPROC oglBlitFramebuffer;
extern PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC oglRenderbufferStorageMultisample;
extern PFNGLFRAMEBUFFERTEXTURELAYERPROC oglFramebufferTextureLayer;


#endif
