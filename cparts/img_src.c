// ImageSource - image sources.

// TODO: supply registration functions that enable support for
// each image type.

struct ImageData;
struct Resource;

typedef bool (*ImageDataSink)(void* self, struct ImageData* data);

struct ImageSource {
	size_t refs;
	bool (*push)(ImageSource* self, ImageDataSink sink, void* obj);
}

ImageSource* imgsrc_create_png(struct Resource* resource);
ImageSource* imgsrc_create_tga(struct Resource* resource);
ImageSource* imgsrc_create_bmp(struct Resource* resource);
ImageSource* imgsrc_create_jpg(struct Resource* resource);
ImageSource* imgsrc_create_psd(struct Resource* resource);
