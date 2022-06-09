
// Software Rasteriser

typedef struct SoftwareRasteriser;

struct SoftwareRasteriser {
	int foo;
};

SoftwareRasteriser* software_rasteriser_create();
void software_rasteriser_destroy(SoftwareRasteriser* swr);

// begin capturing a scene via Renderer interface.
Renderer* software_rasteriser_begin(SoftwareRasteriser* swr);
void software_rasteriser_render_to(SoftwareRasteriser* swr, iPair origin, Surface* output);
void software_rasteriser_end(SoftwareRasteriser* swr);
