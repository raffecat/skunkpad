// JPEG Library

#include "defs.h"
#include "surface.h"
#include "lib_jpeg.h"

//#include "jpeglib.h" // libjpeg header

#include <stdio.h>

export bool load_jpeg(SurfaceData* sd, const char* filename)
{
	FILE *fp;

	if ((fp = fopen(filename, "rb")) == NULL)
		return false;



	/* Close the file */
	fclose(fp);

	/* That's it */
	return true;
}
