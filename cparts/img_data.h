#ifndef CPART_IMAGE_DATA
#define CPART_IMAGE_DATA


#define REF_FIELDS int refs; void (*release)(void* p)


// ImageData - borrowed buffer pattern.

typedef enum ImageFormat { img_fmt_none=0, img_fmt_lum, img_fmt_lum_alpha,
						   img_fmt_rgb, img_fmt_rgba } ImageFormat;

typedef struct ImageData { ImageFormat format; size_t width; size_t height;
						   size_t stride; void* data; } ImageData;

typedef struct ImageDataRef { ImageData img; REF_FIELDS; } ImageDataRef;


// ImageSource - standard source of image data.

// ImageData 'result' must be initialised as follows:
// * 'format' must be valid; img_fmt_none is interpreted as 'any format'.
// * if the ImageSource requires, width and height must be provided.
// * remaining values are ignored.

// ImageData 'dest' is OPTIONAL.
// If provided, it must be fully initialised and point to WRITABLE memory.

// If the ImageSource chooses to write to 'dest' it will return 'dest' pointer.
// Otherwise it will populate 'result' and return 'result' pointer.
// If no data is available, it will touch neither and return NULL.

typedef ImageData* (*ImageSourceFunc)(void* self, ImageData* result, ImageData* dest);

struct ImageSource;
typedef void (*ImageSourceRelease)(struct ImageSource* d);

typedef struct zzImageSource { ImageSourceFunc source;
							 ImageSourceRelease release; } zzImageSource;



#endif
