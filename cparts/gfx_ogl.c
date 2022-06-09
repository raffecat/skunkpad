import * from defs
import * from graphics
import * from gl_impl
import * from gl_impl


// TODO: detect these extensions:

// ARB/EXT_texture_non_power_of_two : full NPOT support (wrap, mipmap)
// ARB/EXT_texture_rectangle : only clamped, no-mipmap NPOT support.


// GfxImage implementation.

/* the only color-renderable internal formats:
ALPHA, LUMINANCE, LUMINANCE_ALPHA, INTENSITY, RED, RG, RGB, RGBA,
FLOAT_R_NV, FLOAT_RG_NV, FLOAT_RGB_NV, and FLOAT_RGBA_NV */

const c_ogl_renderable[] = { 0, GL_ALPHA, GL_LUMINANCE_ALPHA, GL_RGB, GL_RGBA };
const c_ogl_internal[] = { 0, GL_ALPHA8, GL_LUMINANCE8_ALPHA8, GL_RGB8, GL_RGBA8 };
const c_ogl_format[] = { 0, GL_ALPHA, GL_LUMINANCE_ALPHA, GL_RGB, GL_RGBA };

// "Use PBO to upload multiple parts of the texture simultaneously
//  and asynchronously."

// GL_TEXTURE_RECTANGLE_ARB

// you can upload the texture in another thread with another context,
// sharing that texture with your main context.

// upscale a source image to the nearest powers of two and upload
// without generating mipmaps.
bool ogl_img_upscale_pow2(SurfaceData* sd)
{
	int width, height, bytespp;
	int pow_width = 0, pow_height = 0;
	byte* temp;
	GLenum err;

	// TODO: need to scale or pad the image in X and Y depending
	// on flags gfxImgWrapX and gfxImgWrapY (then do nothing in the
	// case that width or height are already POT.)

	// determine the smallest power of two >= the image width
	width = sd->width - 1;
	while (width) { width >>= 1; ++pow_width; }
	width = 1 << pow_width;
	if (width < 1) width = 1;

	// determine the smallest power of two >= the image height
	height = sd->height - 1;
	while (height) { height >>= 1; ++pow_height; }
	height = 1 << pow_height;
	if (height < 1) height = 1;

	bytespp = surfaceBytesPerPixel(sd->format);

	temp = cpart_alloc(width * height * bytespp + 4);
	if (!temp) return false;

	// TEST: looking for corruption.
	temp[width * height * bytespp + 0] = 'S';
	temp[width * height * bytespp + 1] = 'A';
	temp[width * height * bytespp + 2] = 'F';
	temp[width * height * bytespp + 3] = 'E';

	// TODO: work out alignment correctly.
	glPixelStorei(GL_PACK_ALIGNMENT, 1);

	// this should use the current pack and unpack modes.
	err = gluScaleImage(c_ogl_format[sd->format], sd->width, sd->height,
		GL_UNSIGNED_BYTE, sd->data, width, height, GL_UNSIGNED_BYTE, temp);
	if (err) {
		cpart_free(temp);
		return false;
	}

	assert(temp[width * height * bytespp + 0] == 'S');
	assert(temp[width * height * bytespp + 1] == 'A');
	assert(temp[width * height * bytespp + 2] == 'F');
	assert(temp[width * height * bytespp + 3] == 'E');

	// TODO: work out alignment correctly.
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	// must clear unpack row length set up for gluScaleImage.
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

	glTexImage2D(GL_TEXTURE_2D, 0, c_ogl_internal[sd->format], width, height,
		0, c_ogl_format[sd->format], GL_UNSIGNED_BYTE, temp);

	cpart_free(temp);
	return true;
}

void ogl_img_size_changed(OGLImageImpl* tex, int width, int height)
{
	tex->width = width; tex->height = height;
	tex->flags &= ~(gfxImageFlagIsPotX | gfxImageFlagIsPotY);
	// check if width and height are powers of two.
	width = tex->width;
	while ((width & 1) == 0) width >>= 1;
	if (width == 1) tex->flags |= gfxImageFlagIsPotX;
	height = tex->height;
	while ((height & 1) == 0) height >>= 1;
	if (height == 1) tex->flags |= gfxImageFlagIsPotY;
}

bool ogl_img_prepare_upload(OGLImageImpl* tex, SurfaceData* sd)
{
	if (sd->format < 1 || sd->format > 4)
		return false;

	// generate a texture name if we don't have one.
	if (!tex->id) {
		glGenTextures(1, &tex->id);
		if (!tex->id)
			return false;
	}

	// note: glActiveTexture specifies which unit is re-bound here.
	glBindTexture(GL_TEXTURE_2D, tex->id);

	// TODO: work out alignment correctly.
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	// specify row length in pixels to support SurfaceData with
	// a large stride, e.g. a sub-rect of an image.
	glPixelStorei(GL_UNPACK_ROW_LENGTH,
		sd->stride / surfaceBytesPerPixel(sd->format));

	return true;
}

bool ogl_img_upload(GfxImage ifptr, SurfaceData* sd, GfxImageFlags flags) {
	OGLImageImpl* tex = GfxImageToImpl(ifptr);
	// TODO: use glTexSubImage2D if texture has been loaded already?
	tex->flags = flags & (gfx_imageFlagUnused-1); // ignore extra bits.
	if (ogl_img_prepare_upload(tex, sd))
	{
		// TODO: non-power-of-two textures are supported if the GL version
		// is 2.0 or greater, or if the implementation exports the
		// GL_ARB_texture_non_power_of_two extension.
		ogl_img_size_changed(tex, sd->width, sd->height);

		// TODO: allow the use of compressed texture formats.

		// push up texture priority for textures.
		//{GLclampf priority = 1.0f;
		//glPrioritizeTextures(1, &tex->id, &priority);}

		// set mipmap generation for this texture.
		{GLenum mode = (tex->flags & gfxImgMipMap) ? GL_TRUE : GL_FALSE;
		 glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, mode);}

		// TODO:
		// if backend supports RECT and neither gfxImgWrap specified:
		//    create RECT texture and upload.
		//    flag the image for RECT-mode rendering.
		// else if backend supports -hardware- NPOT:
		//    create standard NPOT texture and upload.
		// else:
		//    allocate temp surface.
		//    stretch or pad source into dest (bi-linear or bi-cubic)
		//    duplicate the top and right edges and corner pixel.
		// if no generate mipmap support or want bi-cubic mipmaps:
		//    allocate temp surface (or re-use)
		//    apply bi-linear or bi-cubic filter (SSE2)
		//    upload RECT or standard mipmap level.

		// can use glGenerateMipmap after upload (if GL_ARB_framebuffer_object
		// or OpenGL 3.0+) but must enable GL_TEXTURE_2D to avoid ATI bug.
		if ((tex->flags & gfxImgMipMap) && 0 /* no auto mipmap generation*/) {
			// this is a last resort; not recommended.
			//gluBuild2DMipmaps(GL_TEXTURE_2D, c_ogl_format[sd->format],
			//  sd->width, sd->height,
			//	c_ogl_format[sd->format], GL_UNSIGNED_BYTE, sd->data);
		} else if ((tex->flags & gfxImageFlagIsPotBoth) == gfxImageFlagIsPotBoth) {
			GLenum internal;
			if (flags & gfxImageRenderTarget) internal = c_ogl_renderable[sd->format];
			else internal = c_ogl_internal[sd->format];
			// upload the data without modification.
			glTexImage2D(GL_TEXTURE_2D, 0, internal,
				sd->width, sd->height, 0, c_ogl_format[sd->format],
				GL_UNSIGNED_BYTE, sd->data);
		} else {
			// upscale to the nearest power of two and upload.
			ogl_img_upscale_pow2(sd);
		}

		// must restore this for libraries (e.g. GLU) and other code.
		// see ogl_img_prepare_upload.
		glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE ); // GL_CLAMP | GL_REPEAT
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE ); // GL_CLAMP | GL_REPEAT

		// NB. GL_NEAREST_MIPMAP_LINEAR requires actual mipmaps.
		if (flags & gfxImageMagNearest)
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		else
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		if (flags & gfxImageMinNearest)
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		else
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

		return true;
	}
	return false;
}

bool ogl_img_create(GfxImage ifptr, GfxImageFormat format, unsigned int width, unsigned int height, RGBA col, int flags) {
	OGLImageImpl* self = GfxImageToImpl(ifptr);
	if (flags & gfxImageDoNotFill) {
		GLenum internal;

		self->flags = flags & (gfx_imageFlagUnused-1); // ignore extra bits.

		if (format < 1 || format > 4)
			return false;

		// generate a texture name if we don't have one.
		if (!self->id) {
			glGenTextures(1, &self->id);
			if (!self->id)
				return false;
		}

		// glActiveTexture specifies which unit is re-bound here.
		glBindTexture(GL_TEXTURE_2D, self->id);

		// detect non-power-of-two sizes.
		ogl_img_size_changed(self, width, height);

		// set mipmap generation for this texture.
		{GLenum mode = (flags & gfxImgMipMap) ? GL_TRUE : GL_FALSE;
		 glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, mode);}

		if (flags & gfxImageRenderTarget) internal = c_ogl_renderable[format];
		else internal = c_ogl_internal[format];

		// allocate texture memory without data.
		glTexImage2D(GL_TEXTURE_2D, 0, internal,
			width, height, 0, c_ogl_format[format],
			GL_UNSIGNED_BYTE, 0);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );

		// NB. GL_NEAREST_MIPMAP_LINEAR requires actual mipmaps.
		if (flags & gfxImageMagNearest)
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		else
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		if (flags & gfxImageMinNearest)
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		else
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

		return true;
	}
	else if (format == 4) {
		// This is a horrible way to create a filled texture.
		bool res;
		size_t i, size = width * height;
		RGBA* data = cpart_alloc(size * surfaceBytesPerPixel(format));
		SurfaceData sd = { format, width, height,
			width * surfaceBytesPerPixel(format), (byte*)data };
		for (i=0; i<size; i++) {
			data[i] = col;
		}
		res = ogl_img_upload(ifptr, &sd, flags);
		cpart_free(data);
		return res;
	}
	return false;
}

void ogl_img_update(GfxImage ifptr, int x, int y, SurfaceData* sd)
{
	OGLImageImpl* tex = GfxImageToImpl(ifptr);
	if (ogl_img_prepare_upload(tex, sd))
	{
		int width = min(sd->width, tex->width - x);
		int height = min(sd->height, tex->height - y);
		assert(width > 0 && height > 0);

		glTexSubImage2D(GL_TEXTURE_2D, 0, x, y,
			width, height, c_ogl_format[sd->format],
			GL_UNSIGNED_BYTE, sd->data);

		// must restore this for libraries (e.g. GLU) and other code.
		// see ogl_img_prepare_upload.
		glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	}
}

iPair ogl_img_getSize(GfxImage ifptr) {
	OGLImageImpl* self = GfxImageToImpl(ifptr);
	iPair size = { self->width, self->height };
	return size;
}

void ogl_img_read(GfxImage ifptr, SurfaceData* sd) {
	OGLImageImpl* self = GfxImageToImpl(ifptr);
	if (self->id) {
		// glActiveTexture specifies which unit is re-bound here.
		glBindTexture(GL_TEXTURE_2D, self->id);

		// TODO: work out alignment correctly.
		glPixelStorei(GL_PACK_ALIGNMENT, 1);

		// specify row length in pixels to support SurfaceData with
		// a large stride, e.g. a sub-rect of an image.
		glPixelStorei(GL_PACK_ROW_LENGTH, 
			sd->stride / surfaceBytesPerPixel(sd->format));
		
		// copy the image data.
		assert(sd->format <= 4);
		glGetTexImage(GL_TEXTURE_2D, 0, c_ogl_format[sd->format],
			GL_UNSIGNED_BYTE, sd->data);

		// must restore this for libraries (e.g. GLU) and other code.
		glPixelStorei(GL_PACK_ROW_LENGTH, 0);
	}
}

void ogl_img_release(OGLImageImpl* tex) {
	glDeleteTextures(1, &tex->id);
	tex->id = 0;
	tex->width = 0; tex->height = 0; // invariant.
}

void ogl_img_destruct(GfxImage ifptr) {
	OGLImageImpl* self = GfxImageToImpl(ifptr);
	ogl_img_release(self);
	// TODO remove from linked list, then cpart_free(self).
}

GfxImage ogl_image_i = {
	offsetof(OGLImageImpl, refs) - offsetof(OGLImageImpl, image_i),
	ogl_img_destruct,
	ogl_img_upload,
	ogl_img_create,
	ogl_img_update,
	ogl_img_getSize,
	ogl_img_read,
};

OGLImageImpl* ogl_img_new()
{
	OGLImageImpl* tex = cpart_new(OGLImageImpl);
	tex->image_i = &ogl_image_i;
	tex->refs = 1;
	tex->flags = 0;
	tex->width = tex->height = 0;
	tex->id = 0;
	return tex;
}


// GfxShader implementation.

#define oglShaderMaxImages 8

enum oglShaderFlags {
	oglShaderBlend = 1,
};

struct OGLShaderImpl {
	GfxShader_i* shader_i;
	size_t refs;
	int flags;
	GLuint progId, vpId, fpId;
	GLenum blendSrc, blendDst;
	GfxImage images[oglShaderMaxImages];
	OGLShaderImpl* next;
};

#define GfxShaderToImpl(PTR) ((OGLShaderImpl*)(PTR))

void ogl_sdr_destruct(GfxShader ifptr) {
	OGLShaderImpl* self = GfxShaderToImpl(ifptr);
	{int i; for (i=0; i<oglShaderMaxImages; i++)
		if (self->images[i])
			release(self->images[i]);
	}
}

GLenum c_ogl_blend_func[] = { // GfxBlendFactor -> GLenum
	GL_ZERO, GL_ONE, GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR,
	GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_DST_ALPHA,
	GL_ONE_MINUS_DST_ALPHA, GL_DST_COLOR, GL_ONE_MINUS_DST_COLOR
};

void ogl_sdr_setBlendFunc(GfxShader ifptr, GfxBlendFactor srcFactor, GfxBlendFactor dstFactor)
{
	OGLShaderImpl* self = GfxShaderToImpl(ifptr);
	self->blendSrc = c_ogl_blend_func[srcFactor];
	self->blendDst = c_ogl_blend_func[dstFactor];
	// derive blend enable from blend factors.
	if (srcFactor == gfxOne && dstFactor == gfxZero)
		self->flags |= oglShaderBlend;
	else
		self->flags &= ~oglShaderBlend;
}

void ogl_sdr_bindImage(GfxShader ifptr, unsigned int param, GfxImage image)
{
	OGLShaderImpl* self = GfxShaderToImpl(ifptr);
	if (param < oglShaderMaxImages) {
		self->images[param] = image;
		retain(image);
	}
}

GfxShader_i ogl_shader_i = {
	offsetof(OGLShaderImpl, refs) - offsetof(OGLShaderImpl, shader_i),
	ogl_sdr_destruct,
	ogl_sdr_setBlendFunc,
	ogl_sdr_bindImage,
};

OGLShaderImpl* ogl_shader_new()
{
	OGLShaderImpl* self = cpart_new(OGLShaderImpl);
	self->shader_i = &ogl_shader_i;
	self->refs = 1;
	self->flags = 0;
	self->progId = self->vpId = self->fpId = 0;
	self->blendSrc = GL_ONE;  self->blendDst = GL_ZERO;
	{int i; for (i=0; i<oglShaderMaxImages; i++) self->images[i] = 0;}
	return self;
}


// GfxRender implementation.
// uses OGLImageImpl.

void ogl_r_select(GfxRender ifptr, GfxImage image)
{
	OGLRenderImpl* self = GfxRenderToImpl(ifptr);
	OGLImageImpl* tex;

	if (!self->selectedImg)
		glEnable(GL_TEXTURE_2D);

	// avoid re-binding the same texture - is this worth doing?
	if (image != self->selectedImg) {
		self->selectedImg = image;

		// bind the OpenGL texture object.
		tex = GfxImageToImpl(image);
		glBindTexture(GL_TEXTURE_2D, tex->id);
	}
}

void ogl_r_selectNone(GfxRender ifptr)
{
	OGLRenderImpl* self = GfxRenderToImpl(ifptr);
	if (self->selectedImg != 0) {
		self->selectedImg = 0;
		glDisable(GL_TEXTURE_2D);
	}
}

void ogl_r_quads2d(GfxRender ifptr, int num, const float* verts, const float* coords)
{
	OGLRenderImpl* self = GfxRenderToImpl(ifptr);
	// avoid unnecessary enable calls.
	if (!(self->enabled & ogl_en_vert_a)) {
		self->enabled |= ogl_en_vert_a;
		glEnableClientState(GL_VERTEX_ARRAY);
	}
	if (coords) {
		// avoid unnecessary enable calls.
		if (!(self->enabled & ogl_en_tex0_a)) {
			self->enabled |= ogl_en_tex0_a;
			glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		}
		// set texture coords pointer.
		glTexCoordPointer(2, GL_FLOAT, 0, coords);
	} else {
		// avoid unnecessary disable calls.
		if ((self->enabled & ogl_en_tex0_a)) {
			self->enabled &= ~ogl_en_tex0_a;
			glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		}
	}
	// set vertex array pointer.
	glVertexPointer(2, GL_FLOAT, 0, verts);
	// render the quads.
	glDrawArrays(GL_QUADS, 0, 4*num);
}

void ogl_r_vertexCol(GfxRender ifptr, float red, float green, float blue, float alpha)
{
	//OGLRenderImpl* self = GfxRenderToImpl(ifptr);
	glColor4f(red, green, blue, alpha);
}

void ogl_r_blendFunc(GfxRender ifptr, GfxBlendFactor src, GfxBlendFactor dst)
{
	//OGLRenderImpl* self = GfxRenderToImpl(ifptr);
	if (src == gfxOne && dst == gfxZero) {
		glDisable(GL_BLEND);
	} else {
		glEnable(GL_BLEND);
		glBlendFunc(c_ogl_blend_func[src], c_ogl_blend_func[dst]);
	}
}

void ogl_r_transform2d(GfxRender ifptr, const Affine2D* t)
{
	// NB. base vectors are contiguous in OpenGL.
	float mat[16] = {
		t->Ux, t->Uy, 0, 0, // X basis
		t->Vx, t->Vy, 0, 0, // Y basis
		    0,     0, 1, 0, // unit Z basis
		t->Tx, t->Ty, 0, 1  // translation
	};
	glLoadMatrixf(mat);
}

void ogl_r_clearTransform(GfxRender ifptr)
{
	glLoadIdentity();
}

void ogl_r_clear(GfxRender ifptr, const GfxCol* col)
{
	//OGLRenderImpl* self = GfxRenderToImpl(ifptr);
	glClearColor(col->r, col->g, col->b, col->a);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
}

void ogl_r_setShader(GfxRender ifptr, GfxShader shader)
{
	OGLShaderImpl* sh = GfxShaderToImpl(shader);
}

void ogl_r_drawQuadsPtr(GfxRender ifptr, int numQuads, int vertexFormat, const void* vertexData)
{
}

void ogl_r_drawQuadsIndPtr(GfxRender ifptr, int numQuads, int vertexFormat, const void* vertexData, int indexFormat, const void* indexData)
{
}

void ogl_r_drawTrisIndPtr(GfxRender ifptr, int numTris, int vertexFormat, const void* vertexData, int indexFormat, const void* indexData)
{
}

void ogl_r_drawStripIndPtr(GfxRender ifptr, int numTris, int vertexFormat, const void* vertexData, int indexFormat, const void* indexData)
{
}

void ogl_r_copyToImage(GfxRender ifptr, const iRect* srcRect, GfxImage destImage, const iRect* destRect)
{
}

void ogl_r_setRenderTarget(GfxRender ifptr, GfxImage target)
{
	OGLRenderImpl* self = GfxRenderToImpl(ifptr);

	// FBO support: just activate the FBO and bind the colour attachment
	// point; we don't need a depth buffer. gfxImageRenderTarget will cause
	// the texture to have a compatible internal format. When another target
	// is selected, rebind the colour attachment or deactivate the FBO.
	// If the image cannot be attached, we will fail.

	if (!ogl_framebuffer_object)
		return; // requires FBO support.

	// retain the new target in the render state.
	// retain before release in case they are the same object.
	if (target)
		retain(target);
	if (self->target)
		release(self->target);
	self->target = target;

	if (target)
	{
		// set the rendering target to the supplied image.
		OGLImageImpl* targ = GfxImageToImpl(target);

		if (!self->fbo)
			oglGenFramebuffers(1, &self->fbo);

		oglBindFramebuffer(GL_FRAMEBUFFER, self->fbo);

		oglFramebufferTexture2D(GL_FRAMEBUFFER,
			GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, targ->id, 0);

		self->width = targ->width;
		self->height = targ->height;

		glViewport(0, 0, targ->width, targ->height);
	}
	else
	{
		// restore the window system target.

		oglBindFramebuffer(GL_FRAMEBUFFER, 0);

		self->width = self->cx->width;
		self->height = self->cx->height;

		glViewport(0, 0, self->cx->width, self->cx->height);
	}

	// Pbuffer support: gfxImageVolatile images are just texture names to
	// be bound to a pbuffer's colour buffer. Activate the pbuffer here
	// and stash the image in context; when another target is selected we will
	// bind the colour buffer to the texture. NB. when discard() is called
	// on the texture we can unbind the colour buffer and re-use it.
	// For non-volatile images, use a pbuffer at least as large, and
	// CopyTexSubImage afterward.
	// NB. volatile images reserve framebuffer memory permanently.
}

GfxRender_i ogl_render_i = {
	ogl_r_select,
	ogl_r_selectNone,
	ogl_r_quads2d,
	ogl_r_vertexCol,
	ogl_r_blendFunc,
	ogl_r_transform2d,
	ogl_r_clearTransform,
	ogl_r_clear,
	ogl_r_setRenderTarget,
	/*
	ogl_r_setShader,
	ogl_r_drawQuadsPtr,
	ogl_r_drawQuadsIndPtr,
	ogl_r_drawTrisIndPtr,
	ogl_r_drawStripIndPtr,
	ogl_r_copyToImage,
	*/
};

void gfx_ogl_render_init(OGLRenderImpl* self, OGLContextImpl* cx)
{
	self->render_i = &ogl_render_i;
	self->enabled = 0;
	self->selectedImg = 0;
	self->target = 0;
	self->fbo = 0;
	self->width = self->height = 0;
	self->cx = cx;
}


// GfxContext implementation.

GfxImage ogl_ctx_createImage(GfxContext ifptr)
{
	OGLContextImpl* self = GfxContextToImpl(ifptr);
	OGLImageImpl* tex = ogl_img_new();
	tex->next = self->images; self->images = tex; // linked list.
	return &tex->image_i;
}

GfxTarget ogl_ctx_createTarget(GfxContext ifptr, GfxImageFormat format,
	unsigned int width, unsigned int height, unsigned int samples,
	GfxTargetFlags flags)
{
	OGLContextImpl* self = GfxContextToImpl(ifptr);
	OGLTargetImpl* t = gfx_ogl_target_new(self, width, height, flags);
	if (t) {
		t->next = self->targets; self->targets = t; // linked list.
		return &t->target_i;
	}
	return 0;
}

GfxShader ogl_ctx_createShader(GfxContext ifptr)
{
	OGLContextImpl* self = GfxContextToImpl(ifptr);
	OGLShaderImpl* shader = ogl_shader_new();
	shader->next = self->shaders; self->shaders = shader; // linked list.
	return &shader->shader_i;
}

GfxRender ogl_ctx_getRenderer(GfxContext ifptr)
{
	OGLContextImpl* self = GfxContextToImpl(ifptr);
	return &self->render.render_i;
}

iPair ogl_ctx_getTargetSize(GfxContext ifptr)
{
	OGLContextImpl* self = GfxContextToImpl(ifptr);
	iPair size = { self->render.width, self->render.height };
	return size;
}

GfxContext_i ogl_context_i = {
	ogl_ctx_createImage,
	ogl_ctx_createTarget,
	ogl_ctx_createShader,
	ogl_ctx_getRenderer,
	ogl_ctx_getTargetSize,
};


// GfxBackend implementation.

void ogl_be_init(GfxBackend ifptr)
{
	OGLContextImpl* self = GfxBackendToImpl(ifptr);
	// initialise extensions.
	ogl_init_framebuffer_object();
	ogl_init_render_texture();
	// initialise the scene with the context.
	if (self->scene) {
		(*self->scene)->init(self->scene, &self->context_i);
		if (self->width && self->height)
			(*self->scene)->sized(self->scene, self->width, self->height);
	}
}

void ogl_be_final(GfxBackend ifptr) {
	OGLContextImpl* self = GfxBackendToImpl(ifptr);
	// finalise the scene.
	if (self->scene)
		(*self->scene)->final(self->scene);
	// delete all remaining images.
	{OGLImageImpl* walk = self->images;
	 while (walk) { ogl_img_release(walk); walk = walk->next; }}
}

void ogl_be_sized(GfxBackend ifptr, int width, int height)
{
	OGLContextImpl* self = GfxBackendToImpl(ifptr);
	// save the size of the system framebuffer.
	self->width = width;
	self->height = height;
	// update the render state unless a render target is attached.
	if (!self->render.target) {
		self->render.width = width;
		self->render.height = height;
	}
	// resize the scene.
	if (self->scene && width > 0 && height > 0)
		(*self->scene)->sized(self->scene, width, height);
}

void ogl_be_render(GfxBackend ifptr)
{
	OGLContextImpl* self = GfxBackendToImpl(ifptr);
	// render the scene.
	if (self->scene)
		(*self->scene)->render(self->scene, &self->render.render_i);

	/* disable any functions left enabled.
	if (self->enabled & ogl_en_tex2d) glDisable(GL_TEXTURE_2D);
	if (self->enabled & ogl_en_vert_a) glDisableClientState(GL_VERTEX_ARRAY);
	if (self->enabled & ogl_en_tex0_a) glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	self->enabled &= ~(ogl_en_tex2d|ogl_en_vert_a|ogl_en_tex0_a);
	*/
}

void ogl_be_update(GfxBackend ifptr, GfxDelta delta)
{
	OGLContextImpl* self = GfxBackendToImpl(ifptr);
	// update the scene.
	if (self->scene)
		(*self->scene)->update(self->scene, delta);
}

void ogl_be_setScene(GfxBackend ifptr, GfxScene scene)
{
	OGLContextImpl* self = GfxBackendToImpl(ifptr);
	// finalise the previous scene.
	if (self->scene)
		(*self->scene)->final(self->scene);
	// replace the scene.
	self->scene = scene;
	// initialise the new scene with the context.
	if (scene) {
		(*scene)->init(scene, &self->context_i);
		if (self->width && self->height)
			(*self->scene)->sized(self->scene, self->width, self->height);
	}
}

GfxContext ogl_be_getContext(GfxBackend ifptr)
{
	OGLContextImpl* self = GfxBackendToImpl(ifptr);
	return &self->context_i;
}

GfxBackend_i ogl_backend_i = {
	ogl_be_init,
	ogl_be_final,
	ogl_be_sized,
	ogl_be_render,
	ogl_be_update,
	ogl_be_setScene,
	ogl_be_getContext,
};


// OGLContextImpl factory.

GfxBackend createGfxOpenGLBackend()
{
	OGLContextImpl* self = cpart_new(OGLContextImpl);
	gfx_ogl_render_init(&self->render, self);
	self->context_i = &ogl_context_i;
	self->backend_i = &ogl_backend_i;
	self->scene = 0;
	self->images = 0;
	self->targets = 0;
	self->shaders = 0;
	return &self->backend_i;
}


// OpenGL Extension string.

#include <string.h>

/* http://www.opengl.org/resources/features/OGLextensions/ */
bool ogl_extension_supported(const char* extensions, const char* name) {
	const char *start;
	char *where, *terminator;

	start = extensions;
	for (;;) {
		where = strstr(start, name);
		if (!where)
			break;
		terminator = where + strlen(name);
		if (where == start || *(where - 1) == ' ')
			if (*terminator == ' ' || *terminator == '\0')
				return true;
		start = terminator;
	}
	return false;
}

bool ogl_has_extension(const char *name) {
	return ogl_extension_supported(
		(const char*) glGetString(GL_EXTENSIONS), name);
}

const char* ogl_get_wgl_extensions() {
	const GLubyte* extensions = 0;
	// first check for WGL_ARB_extensions_string.
	PFNWGLGETEXTENSIONSSTRINGARBPROC wglGetExtensionsStringARB;
	wglGetExtensionsStringARB = (PFNWGLGETEXTENSIONSSTRINGARBPROC)
		wglGetProcAddress("wglGetExtensionsStringARB");
	if (wglGetExtensionsStringARB)
		extensions = wglGetExtensionsStringARB(wglGetCurrentDC());
	if (!extensions) {
		// next try EXT_extensions_string.
		PFNWGLGETEXTENSIONSSTRINGEXTPROC wglGetExtensionsStringEXT;
		wglGetExtensionsStringEXT = (PFNWGLGETEXTENSIONSSTRINGEXTPROC)
			wglGetProcAddress("wglGetExtensionsString");
		if (wglGetExtensionsStringEXT)
			extensions = wglGetExtensionsStringEXT();
		if (!extensions) {
			// finally fall back to the standard extension string.
			extensions = glGetString(GL_EXTENSIONS);
		}
	}
	return (const char*) extensions;
}

bool ogl_has_wgl_extension(const char* name) {
	return ogl_extension_supported(
		ogl_get_wgl_extensions(), name);
}

// I'm not sure if this is necessary or not; is it possible that
// some EXT extensions are advertised via EXT_extensions_string and
// are missing from WGL_ARB_extensions_string?

const char* ogl_get_ext_extensions() {
	const GLubyte* extensions = 0;
	// check for EXT_extensions_string support.
	PFNWGLGETEXTENSIONSSTRINGEXTPROC wglGetExtensionsStringEXT;
	wglGetExtensionsStringEXT = (PFNWGLGETEXTENSIONSSTRINGEXTPROC)
		wglGetProcAddress("wglGetExtensionsStringEXT");
	if (wglGetExtensionsStringEXT)
		extensions = wglGetExtensionsStringEXT();
	if (!extensions) {
		// fall back to the standard extension string.
		extensions = glGetString(GL_EXTENSIONS);
	}
	return (const char*) extensions;
}

bool ogl_has_ext_extension(const char* name) {
	return ogl_extension_supported(
		ogl_get_ext_extensions(), name);
}
