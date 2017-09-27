#ifndef CPART_SHAPES
#define CPART_SHAPES


// Geometric shapes

// lines: wu antialiased with alpha from thickness when <= 1 pixel.
// arcs: wu shaded arc with alpha from thickness when <= 1 pixel.

// pixel-centered circle and ellipse antialiased, with sub-pixel offset.

typedef void (*shape_span_func)(void* data, int x, int y, int n);

void shape_circle_fill(int radius, shape_span_func span, void* data);

// line
// arc

// filled circle
// filled ellipse


// span functions.

void shape_span_fill_rgba(void* data, int x, int y, int n);


// render API.

void shape_circle_fill_rgba(struct SurfaceData* sd, int cx, int cy, int radius, RGBA colour);
void shape_circle_fill_rgba16(struct SurfaceData* sd, int cx, int cy, int radius, RGBA16 colour);


#endif
