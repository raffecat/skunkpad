// Span rasterizer.

typedef struct SpanSource SpanSource;
typedef __declspec(align(16)) struct SpanBuffer SpanBuffer;
typedef struct AffineState AffineState;
typedef struct AffineSource AffineSource;

struct SpanSource {
	bool (*more)(SpanSource* source); // advance to next span; true if spans remain.
	byte* (*read)(SpanSource* source, int len); // request span data from current span.
	int maxSpan; // max span request length.
	int pad; // pad to 16 bytes (IA32)
};

struct SpanBuffer {
	byte data[256]; // 64 RGBA pixels.
	//__m128 align; // force alignment for SSE.
}; // __attribute__ ((aligned (16)));

struct AffineState {
	uint32 x, y; // 16.16 current position in view space.
	uint32 px, py, sx, sy; // 16.16 pixel and span increments (2x2 affine)
};

struct AffineSource {
	SpanSource source;
	SpanBuffer buffer;
	AffineState affine;
};
