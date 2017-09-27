#include "defs.h"
#include "surface.h"
#include "lib_png.h"

#include "png.h" // libpng header


static void PNGAPI
read_png_from_buf(png_structp png_ptr, png_bytep data, png_size_t length)
{
	dataBuf* buf = png_get_io_ptr(png_ptr);
	if (length <= buf->size) {
		memcpy(data, buf->data, length);
		buf->data += length;
		buf->size -= length;
	} else png_error(png_ptr, "Read Error - end of buffer");
}

static bool load_png_impl(SurfaceData* sd, FILE *fp, dataBuf* buf)
{
	png_structp png_ptr;
	png_infop info_ptr;
	unsigned int sig_read = 0;
	png_uint_32 width, height;
	int bit_depth, color_type, interlace_type;
	int sdFormat;

	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (png_ptr == NULL)
		return false;

	info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL) {
		png_destroy_read_struct(&png_ptr, NULL, NULL);
		return false;
	}

	if (setjmp(png_jmpbuf(png_ptr))) {
		/* If we get here, we had a problem reading the file */
		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
		return false;
	}

	if (fp)
		png_init_io(png_ptr, fp);
	else
		png_set_read_fn(png_ptr, buf, read_png_from_buf);

	/* The call to png_read_info() gives us all of the information from the
	* PNG file before the first IDAT (image data chunk).  REQUIRED
	*/
	png_read_info(png_ptr, info_ptr);

	png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type,
		&interlace_type, NULL, NULL);

	/* Tell libpng to strip 16 bit/color files down to 8 bits/color */
	png_set_strip_16(png_ptr);

	/* Extract multiple pixels with bit depths of 1, 2, and 4 from a single
	* byte into separate bytes (useful for paletted and grayscale images).
	*/
	png_set_packing(png_ptr);

	/* Expand paletted colors into true RGB triplets */
	if (color_type == PNG_COLOR_TYPE_PALETTE)
		png_set_palette_to_rgb(png_ptr);

	/* Expand grayscale images to the full 8 bits from 1, 2, or 4 bits/pixel */
	if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
		png_set_expand_gray_1_2_4_to_8(png_ptr);

	/* Expand paletted or RGB images with transparency to full alpha channels
	* so the data will be available as RGBA quartets.
	*/
	if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
		png_set_tRNS_to_alpha(png_ptr);

	if (color_type == PNG_COLOR_TYPE_GRAY)
	{
		sdFormat = surface_a8;

		/* Optional call to gamma correct and add the background to the palette
		* and update info structure.  REQUIRED if you are expecting libpng to
		* update the palette for you (ie you selected such a transform above).
		*/
		png_read_update_info(png_ptr, info_ptr);

		/* Final output format must match SurfaceData format. */
		if (png_get_rowbytes(png_ptr, info_ptr) != width) {
			png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
			return false;
		}
	}
	else
	{
		sdFormat = surface_rgba8; // up-convert to RGBA.

		/* Add filler (or alpha) byte (before/after each RGB triplet) */
		png_set_filler(png_ptr, 0xff, PNG_FILLER_AFTER);

		/* Optional call to gamma correct and add the background to the palette
		* and update info structure.  REQUIRED if you are expecting libpng to
		* update the palette for you (ie you selected such a transform above).
		*/
		png_read_update_info(png_ptr, info_ptr);

		/* Final output format must match SurfaceData format. */
		if (png_get_rowbytes(png_ptr, info_ptr) != width * 4) {
			png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
			return false;
		}
	}

	/* Allocate the memory to hold the image. */
	surface_create(sd, sdFormat, width, height);

	/* The easiest way to read the image: */
	{
		// TODO: use undo-stack allocator.
		png_bytepp row_pointers = calloc(height, sizeof(png_bytep));
		png_uint_32 row;

		for (row = 0; row < height; row++)
			row_pointers[row] = sd->data + (row * sd->stride);

		/* Now it's time to read the image. */
		png_read_image(png_ptr, row_pointers);

		free(row_pointers); // TODO: fix leak on error.
	}

	/* Read rest of file, and get additional chunks in info_ptr */
	png_read_end(png_ptr, info_ptr);

	/* Clean up after the read, and free any memory allocated */
	png_destroy_read_struct(&png_ptr, &info_ptr, NULL);

	/* That's it */
	return true;
}


bool load_png(SurfaceData* sd, const char* filename)
{
	FILE *fp;
	bool ok;

	if ((fp = fopen(filename, "rb")) == NULL)
		return false;

	ok = load_png_impl(sd, fp, 0);

	fclose(fp);

	return ok;
}

bool load_png_buf(SurfaceData* sd, dataBuf buf)
{
	return load_png_impl(sd, 0, &buf);
}
