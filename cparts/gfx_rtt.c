import * from defs
import * from graphics
import * from gl_impl
import * from surface

// FBO notes:

// Use one FBO per texture size (faster than FBO per texture)
// Prefer to batch GfxTarget rendering to avoid FBO switches.

// For ARB, use one depth texture large enough for all sizes;
// For EXT must create a depth texture matching each size.

// Iternally reuse textures and depth/stencil buffers once
// discard() has been called on a target, but only if other
// targets exist with the same size. Internally deallocate the
// buffers after a few frames unless 'persist' flag is set?


// TODO: move functions into function tables, store a context ref
// in each instance so we can track render state changes, expect
// the context to load extensions and hold pointers to the
// function tables grouped by usage (e.g. FBO table, RTT table)
// and perhaps copy that table pointer into each instance.
// (and so we can check extensions when we create a context,
// which happens when we create a pbuffer target; later we can
// build a shared array of function tables per pixel format
// to avoid the slow and redundant work.)
// store a ref to a backend in each context - the backend contains
// globally shared state like the pixel format table.
// backend -> display context -> target -> pbuffer context.


// Frame Buffer Object extentions.

export bool ogl_framebuffer_object = false;
export bool ogl_framebuffer_blit = false;
export bool ogl_framebuffer_multisample = false;
export bool ogl_packed_depth_stencil = false;

// NB. glRenderbufferStorage may return errors for alpha,
// luminance, intensity, luminance_alpha formats when not supported.

// mixed dimensions, mixed colour formats, must gen names,
// glFramebufferTextureLayer, FBOs not shareable across contexts.
export bool ogl_framebuffer_is_arb = false;

// GL_ARB_framebuffer_object
// (EXT_framebuffer_object, EXT_framebuffer_blit,
//  EXT_framebuffer_multisample, EXT_packed_depth_stencil)
export PFNGLBINDRENDERBUFFERPROC oglBindRenderbuffer = 0;
export PFNGLDELETERENDERBUFFERSPROC oglDeleteRenderbuffers = 0;
export PFNGLGENRENDERBUFFERSPROC oglGenRenderbuffers = 0;
export PFNGLRENDERBUFFERSTORAGEPROC oglRenderbufferStorage = 0;
export PFNGLGETRENDERBUFFERPARAMETERIVPROC oglGetRenderbufferParameteriv = 0;
export PFNGLBINDFRAMEBUFFERPROC oglBindFramebuffer = 0;
export PFNGLDELETEFRAMEBUFFERSPROC oglDeleteFramebuffers = 0;
export PFNGLGENFRAMEBUFFERSPROC oglGenFramebuffers = 0;
export PFNGLCHECKFRAMEBUFFERSTATUSPROC oglCheckFramebufferStatus = 0;
export PFNGLFRAMEBUFFERTEXTURE2DPROC oglFramebufferTexture2D = 0;
export PFNGLFRAMEBUFFERRENDERBUFFERPROC oglFramebufferRenderbuffer = 0;
export PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC oglGetFramebufferAttachmentParameteriv = 0;
export PFNGLGENERATEMIPMAPPROC oglGenerateMipmap = 0;
export PFNGLBLITFRAMEBUFFERPROC oglBlitFramebuffer = 0;
export PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC oglRenderbufferStorageMultisample = 0;
export PFNGLFRAMEBUFFERTEXTURELAYERPROC oglFramebufferTextureLayer = 0;

export void ogl_init_framebuffer_object()
{
	ogl_framebuffer_object = (ogl_has_extension("GL_ARB_framebuffer_object") &&
		(oglBindRenderbuffer = (PFNGLBINDRENDERBUFFERPROC) ogl_get_proc("glBindRenderbufferARB")) &&
		(oglDeleteRenderbuffers = (PFNGLDELETERENDERBUFFERSPROC) ogl_get_proc("glDeleteRenderbuffersARB")) &&
		(oglGenRenderbuffers = (PFNGLGENRENDERBUFFERSPROC) ogl_get_proc("glGenRenderbuffersARB")) &&
		(oglRenderbufferStorage = (PFNGLRENDERBUFFERSTORAGEPROC) ogl_get_proc("glRenderbufferStorageARB")) &&
		(oglGetRenderbufferParameteriv = (PFNGLGETRENDERBUFFERPARAMETERIVPROC) ogl_get_proc("glGetRenderbufferParameterivARB")) &&
		(oglBindFramebuffer = (PFNGLBINDFRAMEBUFFERPROC) ogl_get_proc("glBindFramebufferARB")) &&
		(oglDeleteFramebuffers = (PFNGLDELETEFRAMEBUFFERSPROC) ogl_get_proc("glDeleteFramebuffersARB")) &&
		(oglGenFramebuffers = (PFNGLGENFRAMEBUFFERSPROC) ogl_get_proc("glGenFramebuffersARB")) &&
		(oglCheckFramebufferStatus = (PFNGLCHECKFRAMEBUFFERSTATUSPROC) ogl_get_proc("glCheckFramebufferStatusARB")) &&
		(oglFramebufferTexture2D = (PFNGLFRAMEBUFFERTEXTURE2DPROC) ogl_get_proc("glFramebufferTexture2DARB")) &&
		(oglFramebufferRenderbuffer = (PFNGLFRAMEBUFFERRENDERBUFFERPROC) ogl_get_proc("glFramebufferRenderbufferARB")) &&
		(oglGetFramebufferAttachmentParameteriv = (PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC) ogl_get_proc("glGetFramebufferAttachmentParameterivARB")) &&
		(oglGenerateMipmap = (PFNGLGENERATEMIPMAPPROC) ogl_get_proc("glGenerateMipmapARB")) &&
		(oglBlitFramebuffer = (PFNGLBLITFRAMEBUFFERPROC) ogl_get_proc("glBlitFramebufferARB")) &&
		(oglRenderbufferStorageMultisample = (PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC) ogl_get_proc("glRenderbufferStorageMultisampleARB")) &&
		(oglFramebufferTextureLayer = (PFNGLFRAMEBUFFERTEXTURELAYERPROC) ogl_get_proc("glFramebufferTextureLayerARB")) );

	if (ogl_framebuffer_object) {
		ogl_framebuffer_is_arb = ogl_framebuffer_blit =
			ogl_framebuffer_multisample = ogl_packed_depth_stencil = true;
		return; // ARB_framebuffer_object subsumes all of the extensions below.
	}

	ogl_framebuffer_object = (ogl_has_extension("GL_EXT_framebuffer_object") &&
		(oglBindRenderbuffer = (PFNGLBINDRENDERBUFFERPROC) ogl_get_proc("glBindRenderbufferEXT")) &&
		(oglDeleteRenderbuffers = (PFNGLDELETERENDERBUFFERSPROC) ogl_get_proc("glDeleteRenderbuffersEXT")) &&
		(oglGenRenderbuffers = (PFNGLGENRENDERBUFFERSPROC) ogl_get_proc("glGenRenderbuffersEXT")) &&
		(oglRenderbufferStorage = (PFNGLRENDERBUFFERSTORAGEPROC) ogl_get_proc("glRenderbufferStorageEXT")) &&
		(oglGetRenderbufferParameteriv = (PFNGLGETRENDERBUFFERPARAMETERIVPROC) ogl_get_proc("glGetRenderbufferParameterivEXT")) &&
		(oglBindFramebuffer = (PFNGLBINDFRAMEBUFFERPROC) ogl_get_proc("glBindFramebufferEXT")) &&
		(oglDeleteFramebuffers = (PFNGLDELETEFRAMEBUFFERSPROC) ogl_get_proc("glDeleteFramebuffersEXT")) &&
		(oglGenFramebuffers = (PFNGLGENFRAMEBUFFERSPROC) ogl_get_proc("glGenFramebuffersEXT")) &&
		(oglCheckFramebufferStatus = (PFNGLCHECKFRAMEBUFFERSTATUSPROC) ogl_get_proc("glCheckFramebufferStatusEXT")) &&
		(oglFramebufferTexture2D = (PFNGLFRAMEBUFFERTEXTURE2DPROC) ogl_get_proc("glFramebufferTexture2DEXT")) &&
		(oglFramebufferRenderbuffer = (PFNGLFRAMEBUFFERRENDERBUFFERPROC) ogl_get_proc("glFramebufferRenderbufferEXT")) &&
		(oglGetFramebufferAttachmentParameteriv = (PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC) ogl_get_proc("glGetFramebufferAttachmentParameterivEXT")) &&
		(oglGenerateMipmap = (PFNGLGENERATEMIPMAPPROC) ogl_get_proc("glGenerateMipmapEXT")) );

	// requires EXT_framebuffer_object.
	ogl_framebuffer_blit = (
		ogl_has_extension("GL_EXT_framebuffer_blit") &&
		(oglBlitFramebuffer = (PFNGLBLITFRAMEBUFFERPROC) ogl_get_proc("glBlitFramebufferEXT")) );

	// requires EXT_framebuffer_object, EXT_framebuffer_blit.
	ogl_framebuffer_multisample = (
		ogl_has_extension("GL_EXT_framebuffer_multisample") &&
		(oglRenderbufferStorageMultisample = (PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC) ogl_get_proc("glRenderbufferStorageMultisampleEXT")) );

	// this does not depend on any of the EXT extensions above.
	ogl_packed_depth_stencil = ogl_has_extension("GL_EXT_packed_depth_stencil");
}


// Pbuffer render to texture extensions.

bool ogl_render_texture = false;

// WGL_ARB_pixel_format, WGL_EXT_pixel_format
PFNWGLGETPIXELFORMATATTRIBIVARBPROC wglGetPixelFormatAttribiv = 0;
PFNWGLGETPIXELFORMATATTRIBFVARBPROC wglGetPixelFormatAttribfv = 0;
PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormat = 0;

// WGL_ARB_pbuffer, WGL_EXT_pbuffer (binary compatible)
// NB. EXT_pbuffer may not appear in wglGetExtensionsStringARB
// even thogh EXT_pbuffer is supported?
PFNWGLCREATEPBUFFERARBPROC wglCreatePbuffer = 0;
PFNWGLGETPBUFFERDCARBPROC wglGetPbufferDC = 0;
PFNWGLRELEASEPBUFFERDCARBPROC wglReleasePbufferDC = 0;
PFNWGLDESTROYPBUFFERARBPROC wglDestroyPbuffer = 0;
PFNWGLQUERYPBUFFERARBPROC wglQueryPbuffer = 0;

// WGL_ARB_create_context
PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribs = 0;

// WGL_ARB_render_texture
PFNWGLBINDTEXIMAGEARBPROC wglBindTexImage = 0;
PFNWGLRELEASETEXIMAGEARBPROC wglReleaseTexImage = 0;
PFNWGLSETPBUFFERATTRIBARBPROC wglSetPbufferAttrib = 0;

void ogl_init_render_texture()
{
	bool pixel_format = (ogl_has_wgl_extension("WGL_ARB_pixel_format") &&
		(wglGetPixelFormatAttribiv = (PFNWGLGETPIXELFORMATATTRIBIVARBPROC) ogl_get_proc("wglGetPixelFormatAttribivARB")) &&
		(wglGetPixelFormatAttribfv = (PFNWGLGETPIXELFORMATATTRIBFVARBPROC) ogl_get_proc("wglGetPixelFormatAttribfvARB")) &&
		(wglChoosePixelFormat = (PFNWGLCHOOSEPIXELFORMATARBPROC) ogl_get_proc("wglChoosePixelFormatARB")) );

	bool pbuffer = (ogl_has_wgl_extension("WGL_ARB_pbuffer") &&
		(wglCreatePbuffer = (PFNWGLCREATEPBUFFERARBPROC) ogl_get_proc("wglCreatePbufferARB")) &&
		(wglGetPbufferDC = (PFNWGLGETPBUFFERDCARBPROC) ogl_get_proc("wglGetPbufferDCARB")) &&
		(wglReleasePbufferDC = (PFNWGLRELEASEPBUFFERDCARBPROC) ogl_get_proc("wglReleasePbufferDCARB")) &&
		(wglDestroyPbuffer = (PFNWGLDESTROYPBUFFERARBPROC) ogl_get_proc("wglDestroyPbufferARB")) &&
		(wglQueryPbuffer = (PFNWGLQUERYPBUFFERARBPROC) ogl_get_proc("wglQueryPbufferARB")) );

	bool render_texture = (ogl_has_wgl_extension("WGL_ARB_render_texture") &&
		(wglBindTexImage = (PFNWGLBINDTEXIMAGEARBPROC) ogl_get_proc("wglBindTexImageARB")) &&
		(wglReleaseTexImage = (PFNWGLRELEASETEXIMAGEARBPROC) ogl_get_proc("wglReleaseTexImageARB")) &&
		(wglSetPbufferAttrib = (PFNWGLSETPBUFFERATTRIBARBPROC) ogl_get_proc("wglSetPbufferAttribARB")) );

	if (pixel_format && pbuffer && render_texture) {
		ogl_render_texture = true;
		//return;
	}

	pixel_format = (ogl_has_ext_extension("WGL_EXT_pixel_format") &&
		(wglGetPixelFormatAttribiv = (PFNWGLGETPIXELFORMATATTRIBIVARBPROC) ogl_get_proc("wglGetPixelFormatAttribivEXT")) &&
		(wglGetPixelFormatAttribfv = (PFNWGLGETPIXELFORMATATTRIBFVARBPROC) ogl_get_proc("wglGetPixelFormatAttribfvEXT")) &&
		(wglChoosePixelFormat = (PFNWGLCHOOSEPIXELFORMATARBPROC) ogl_get_proc("wglChoosePixelFormatEXT")) );

	pbuffer = (ogl_has_ext_extension("WGL_EXT_pbuffer") &&
		(wglCreatePbuffer = (PFNWGLCREATEPBUFFERARBPROC) ogl_get_proc("wglCreatePbufferEXT")) &&
		(wglGetPbufferDC = (PFNWGLGETPBUFFERDCARBPROC) ogl_get_proc("wglGetPbufferDCEXT")) &&
		(wglReleasePbufferDC = (PFNWGLRELEASEPBUFFERDCARBPROC) ogl_get_proc("wglReleasePbufferDCEXT")) &&
		(wglDestroyPbuffer = (PFNWGLDESTROYPBUFFERARBPROC) ogl_get_proc("wglDestroyPbufferEXT")) &&
		(wglQueryPbuffer = (PFNWGLQUERYPBUFFERARBPROC) ogl_get_proc("wglQueryPbufferEXT")) );

	render_texture = (ogl_has_ext_extension("WGL_EXT_render_texture") &&
		(wglBindTexImage = (PFNWGLBINDTEXIMAGEARBPROC) ogl_get_proc("wglBindTexImageEXT")) &&
		(wglReleaseTexImage = (PFNWGLRELEASETEXIMAGEARBPROC) ogl_get_proc("wglReleaseTexImageEXT")) &&
		(wglSetPbufferAttrib = (PFNWGLSETPBUFFERATTRIBARBPROC) ogl_get_proc("wglSetPbufferAttribEXT")) );

	if (pixel_format && pbuffer && render_texture)
		ogl_render_texture = true;
}

// WGL_ARB_pixel_format_float
// WGL_EXT_depth_float
// WGL_EXT_pixel_format_packed_float
// WGL_EXT_multisample
// WGL_EXT_framebuffer_sRGB
// WGL_NV_render_texture_rectangle (non-POT tex for pbuffer)
// WGL_NV_render_depth_texture (render depth to pbuffer)
// WGL_NV_float_buffer
// WGL_NV_multisample_coverage
// WGL_NV_copy_image
// WGL_ATI_pixel_format_float
// GL_EXT_framebuffer_multisample (then GL_EXT_framebuffer_blit)
// GL_SGIX_depth_texture
// GL_SGIS_Shadow_map

// Using pbuffers:
// Create a pbuffer of desired size, check actual size.
// Create a texture name.
// Make the pbuffer the current render target.
// Render an image.
// Make the window the current render target.
// Bind the pbuffer to the texture.
// Use the texture normally.
// Release the pbuffer from the texture.

// screen_dc = wglGetCurrentDC()
// wglChoosePixelFormat
//   WGL_DRAW_TO_PBUFFER
//   WGL_BIND_TO_TEXTURE_RGB[A]_ARB
//   WGL_BIND_TO_TEXTURE_DEPTH_NV (render depth texture)
//   WGL_BIND_TO_TEXTURE_RECTANGLE_RGB[A]_NV (for non-POT)
// pb = wglCreatePbufferARB:
//   WGL_TEXTURE_FORMAT_ARB, WGL_TEXTURE_RGB[A]_ARB
//   WGL_DEPTH_TEXTURE_FORMAT_NV, WGL_TEXTURE_DEPTH_COMPONENT_NV
//   WGL_TEXTURE_TARGET_ARB, WGL_TEXTURE_2D_ARB
//   WGL_TEXTURE_TARGET_ARB, WGL_TEXTURE_RECTANGLE_NV
//   WGL_MIPMAP_TEXTURE_ARB, 1 (to create mipmap levels)
//   WGL_PBUFFER_LARGEST_ARB, 1 (instead of failing)
// pb_dc = wglGetPbufferDCARB
// pb_rc = wglCreateContext
// wglQueryPbufferARB: WGL_PBUFFER_WIDTH_ARB, WGL_PBUFFER_HEIGHT_ARB

// wglMakeCurrent(pb_dc, pb_rc)
// wglSetPbufferAttribARB: select cube face or mip level
// -render to texture-
// wglMakeCurrent(screen_dc, screen_rc)

// glBindTexture
// wglBindTexImageARB(pb, WGL_FRONT_LEFT_ARB)
// -render using texture-
// wglReleaseTexImageARB(pb, WGL_FRONT_LEFT_ARB)


// Frame Buffer Object backend.

void ogl_targ_fbo_destruct(GfxTarget ifptr) {
	OGLTargetImpl* self = GfxTargetToImpl(ifptr);

	// bind the FBO so that deleted textures and render buffers
	// will unbind themselves from the FBO - is this necessary?
	if (self->fbo)
		oglBindFramebuffer(GL_FRAMEBUFFER, self->fbo);

	if (self->colImg)
		release(self->colImg);
	if (self->colBuf)
		oglDeleteRenderbuffers(1, &self->colBuf);

	if (self->depthImg)
		release(self->depthImg);
	if (self->depthBuf)
		oglDeleteRenderbuffers(1, &self->depthBuf);

	if (self->fbo) {
		oglBindFramebuffer(GL_FRAMEBUFFER, 0); // unbind fbo.
		oglDeleteFramebuffers(1, &self->fbo);
	}

	self->fbo = self->colBuf = self->depthBuf = 0;
	self->colImg = self->depthImg = 0;

	// TODO remove from linked list, then cpart_free(self).
}

GfxRender ogl_targ_fbo_begin(GfxTarget ifptr) {
	OGLTargetImpl* self = GfxTargetToImpl(ifptr);

	oglBindFramebuffer(GL_FRAMEBUFFER, self->fbo);

	// TODO: should do these state changs through the GfxRender
	// interface so the backend can track them.

	// disable colour write unless we have a colour buffer.
	if (!(self->flags & gfxTargetHasColour)) {
		glDrawBuffer(GL_NONE);
	}

	// disable depth test and depth write unless we have a depth buffer.
	if (!(self->flags & gfxTargetHasDepth)) {
		// glDisable(GL_DEPTH_TEST);
		// glDepthMask(GL_FALSE);
	}

	// return our context's render implementation.
	return &self->cx->render.render_i;
}

void ogl_targ_fbo_end(GfxTarget ifptr) {
	OGLTargetImpl* self = GfxTargetToImpl(ifptr);

	// bind back to the system-provided window buffers.
	oglBindFramebuffer(GL_FRAMEBUFFER, 0);

	// TODO: should do these state changs through the GfxRender
	// interface so the backend can track them.

	// re-enable colour buffer rendering.
	glDrawBuffer(GL_BACK);

	// TODO: depends on previously active state.
	// glDepthMask(GL_TRUE);
}

GfxImage ogl_targ_fbo_commit(GfxTarget ifptr) {
	OGLTargetImpl* self = GfxTargetToImpl(ifptr);

	// make sure rendering has ended.
	ogl_targ_fbo_end(ifptr);

	return self->colImg;
}

bool ogl_targ_fbo_resize(GfxTarget ifptr, int width, int height) {
	OGLTargetImpl* self = GfxTargetToImpl(ifptr);
	GLenum status;

	// TODO: round up to the next power of two unless non-POT
	// textures are supported; if texture too large, reduce size
	// until it can be created (if gfxTargetLargestAvailable)

	if (!self->fbo)
		oglGenFramebuffers(1, &self->fbo);

	oglBindFramebuffer(GL_FRAMEBUFFER, self->fbo);

	if (self->flags & gfxTargetHasColour)
	{
		if (self->flags & gfxTargetKeepColour)
		{
			OGLImageImpl* img;

			// create a texture of the requested size.
			if (!self->colImg) {
				RGBA noCol = {0};
				self->colImg = GfxContext_createImage(&self->cx->context_i);
				GfxImage_create(self->colImg, 4, width, height,
					noCol, gfxImageDoNotFill);
			}

			// bind the texture to the FBO as colour target.
			img = GfxImageToImpl(self->colImg);
			oglFramebufferTexture2D(GL_FRAMEBUFFER,
				GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, img->id, 0);
		}
		else
		{
			// create a render buffer instead.
			oglGenRenderbuffers(1, &self->colBuf);
			oglBindRenderbuffer(GL_RENDERBUFFER, self->colBuf);
			oglRenderbufferStorage(GL_RENDERBUFFER,
				GL_RGBA8, width, height);
			oglFramebufferRenderbuffer(GL_FRAMEBUFFER,
				GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, self->colBuf);
		}
	}

	if (self->flags & gfxTargetHasDepth)
	{
		if (self->flags & gfxTargetKeepDepth)
		{
/*
			OGLImageImpl* tex;
			// create a texture of the requested size.
			if (!self->depthImg) {
				RGBA noCol = {0};
				self->depthImg = GfxContext_createImage(self->context);
				GfxImage_create(self->depthImg, gfxFormatDepth, width, height,
					noCol, gfxImageDoNotFill | gfxImageMagNearest | gfxImageMinNearest);
			}

			// TODO unhax to fix up depth texture.
			tex = GfxImageToImpl(self->depthImg);
			glBindTexture(GL_TEXTURE_2D, tex->id);
			glTexParameteri(GL_TEXTURE_2D, GL_DEPTH_TEXTURE_MODE, GL_INTENSITY);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_R_TO_TEXTURE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
			// GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT24,
			// GL_DEPTH_COMPONENT32, GL_DEPTH24_STENCIL8_EXT
			glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32, width, height, 0,
				GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, NULL);

			// bind the texture to the FBO as depth target.
			glFramebufferTexture2D(GL_FRAMEBUFFER,
				GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, tex->id, 0);
*/
		}
		else
		{
			// create a render buffer to hold depth.
			oglGenRenderbuffers(1, &self->depthBuf);
			oglBindRenderbuffer(GL_RENDERBUFFER, self->depthBuf);
			oglRenderbufferStorage(GL_RENDERBUFFER,
				GL_DEPTH_COMPONENT32, width, height);
			oglFramebufferRenderbuffer(GL_FRAMEBUFFER,
				GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, self->depthBuf);
		}
	}

	status = oglCheckFramebufferStatus(GL_FRAMEBUFFER);

	oglBindFramebuffer(GL_FRAMEBUFFER, 0);

	return (status == GL_FRAMEBUFFER_COMPLETE);
}

GfxTarget_i ogl_target_fbo_i = {
	offsetof(OGLTargetImpl, refs) - offsetof(OGLTargetImpl, target_i),
	ogl_targ_fbo_destruct,
	ogl_targ_fbo_begin,
	ogl_targ_fbo_end,
	ogl_targ_fbo_commit,
	ogl_targ_fbo_resize,
};

/*
void ogl_ifbo_destruct(GfxImage ifptr) { // thunk.
	ogl_targ_fbo_destruct(&GfxImageToImpl(ifptr)->target_i);
}

void ogl_ifbo_upload(GfxImage ifptr, struct SurfaceData* sd, GfxImageFlags flags) {
	// cannot upload to a GfxTarget.
}

void ogl_ifbo_create(GfxImage ifptr, int format, int width, int height,
							RGBA col, GfxImageFlags flags) {
	// cannot re-create a GfxTarget.
}

void ogl_ifbo_update(GfxImage ifptr, int x, int y, struct SurfaceData* sd) {
	// cannot update a GfxTarget, although it might be useful.
}

iPair ogl_ifbo_getSize(GfxImage ifptr) {
	OGLTargetImpl* self = GfxImageToImpl(ifptr);
	iPair size = { self->width, self->height };
	return size;
}

GfxImage_i ogl_image_fbo_i = {
	offsetof(OGLTargetImpl, refs) - offsetof(OGLTargetImpl, image_i),
	ogl_ifbo_destruct,
	ogl_ifbo_upload,
	ogl_ifbo_create,
	ogl_ifbo_update,
	ogl_ifbo_getSize,
};
*/


// Pbuffers backend.

void ogl_targ_pbuf_destruct(GfxTarget ifptr) {
	OGLTargetImpl* self = GfxTargetToImpl(ifptr);

	if (self->rc) {
		wglDeleteContext(self->rc);
		wglReleasePbufferDC(self->pbuf, self->dc);
		wglDestroyPbuffer(self->pbuf);
		self->rc = 0;
		self->dc = 0;
		self->pbuf = 0;
	}

	// TODO remove from linked list, then cpart_free(self).
}

GfxRender ogl_targ_pbuf_begin(GfxTarget ifptr) {
	OGLTargetImpl* self = GfxTargetToImpl(ifptr);
	HGLRC hRC;

	// avoid saving and activating if our own context is already
	// current (i.e. double begin)
	// TODO: track this in OGLContext, expect wglGet to be slow.
	hRC = wglGetCurrentContext();
	if (hRC != self->rc)
	{
		// save the active OpenGL context so we can restore it.
		// TODO: track this in OGLContext, expect wglGet to be slow.
		self->prev_dc = wglGetCurrentDC();
		self->prev_rc = hRC;

		// make our pbuffer context current.
		wglMakeCurrent(self->dc, self->rc);
	}

	// return our context's render implementation.
	return &self->cx->render.render_i;
}

void ogl_targ_pbuf_end(GfxTarget ifptr) {
	OGLTargetImpl* self = GfxTargetToImpl(ifptr);

	// do nothing unless we saved a previous context.
	if (self->prev_dc && self->prev_rc)
	{
		// restore the context that was active when begin was called.
		wglMakeCurrent(self->prev_dc, self->prev_rc);

		// clear previous context to avoid double activation.
		self->prev_dc = 0;
		self->prev_rc = 0;
	}
}

GfxImage ogl_targ_pbuf_commit(GfxTarget ifptr) {
	OGLTargetImpl* self = GfxTargetToImpl(ifptr);

	// make sure rendering has ended.
	ogl_targ_pbuf_end(ifptr);

	// do voodoo to bind the pbuffer as a texture?

	return 0;
}

bool ogl_targ_pbuf_resize(GfxTarget ifptr, int width, int height) {
	OGLTargetImpl* self = GfxTargetToImpl(ifptr);

	// TODO: round up to the next power of two unless non-POT
	// textures are supported; if texture too large, reduce size
	// until it can be created (if gfxTargetLargestAvailable)

	// TODO: check implementation defined max sizes.

	if (self->flags & gfxTargetHasColour)
	{
		if (self->flags & gfxTargetKeepColour)
		{
			// create a texture of the requested size.
		}
	}

	if (self->flags & gfxTargetHasDepth)
	{
		if (self->flags & gfxTargetKeepDepth)
		{
			// create a texture of the requested size.
		}
	}


	return true;
}

GfxTarget_i ogl_target_pbuf_i = {
	offsetof(OGLTargetImpl, refs) - offsetof(OGLTargetImpl, target_i),
	ogl_targ_pbuf_destruct,
	ogl_targ_pbuf_begin,
	ogl_targ_pbuf_end,
	ogl_targ_pbuf_commit,
	ogl_targ_pbuf_resize,
};


// GfxTarget factory.

export OGLTargetImpl* gfx_ogl_target_new(OGLContextImpl* context, int width, int height, GfxTargetFlags flags)
{
	OGLTargetImpl* t = cpart_new(OGLTargetImpl);
	if (ogl_framebuffer_object) {
		t->target_i = &ogl_target_fbo_i;
		t->cx = context;
	} else if (ogl_render_texture) {
		t->target_i = &ogl_target_pbuf_i;
		t->cx = 0; // TODO: create new context here.
	} else {
		return 0; // no support.
	}
	t->refs = 1;
	t->flags = flags & (gfx_bufferFlagUnused-1); // ignore extra bits.
	t->width = t->height = 0;
	t->fbo = t->colBuf = t->depthBuf = 0;
	t->colImg = t->depthImg = 0;
	// use the chosen backend to create the target.
	if (t->target_i->resize(&t->target_i, width, height))
		return t;
	// bad luck.
	ogl_targ_fbo_destruct(&t->target_i);
	return 0;
}
