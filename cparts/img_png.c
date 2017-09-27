#include "defs.h"
#include "img_src.h"
#include "img_data.h"
#include "loaders.h"


// PNGImageSource

typedef struct PNGImageSource { IMAGE_SOURCE_FIELDS;
								Resource* resource; } PNGImageSource;

static void imgsrc_png_destroy(Object* p) {
	PNGImageSource* o = (PNGImageSource*)p;
	release(o->resource);
	cpart_free(o);
}

static ObjectType type_imgsrc_png = { "PNGImageSource", imgsrc_png_destroy };

static bool imgsrc_png_push(ImageSource* p, ImageDataSink sink, void* obj) {
	PNGImageSource* o = (PNGImageSource*)p;
	ReadStream rs;
	if (o->resource->read(o->resource, &rs)) {
		do {
		} while (rs.next(&rs));
	}
	return false;
}

ImageSource* imgsrc_create_png(Resource* resource) {
	PNGImageSource* o = cpart_new(PNGImageSource);
	o->type = &type_imgsrc_png;
	o->push = imgsrc_png_push;
	o->resource = retain(resource);
	return (ImageSource*)o;
}
