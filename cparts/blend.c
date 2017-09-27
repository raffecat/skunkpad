
// Blend modes.

export enum BlendMode {
	blendCopy = 0,
	blendNormal,
	blendAdd,			// A + B
	blendSubtract,		// A + B - 1
	blendMultiply,		// A * B
	blendScreen,		// 1-(1-A)*(1-B) or A+(B*(1-A))
	blendDarken,		// min(A,B)
	blendLighten,		// max(A,B)
	blendDifference,	// abs(A-B) or sat(A-B) + sat(B-A)
	blendExclusion,		// A+B-2AB
	blendOverlay,		// 2AB (A < 0.5) 1-2*(1-A)*(1-B)
	blendHardLight,		// 2AB (B < 0.5) 1-2*(1-A)*(1-B)
	blendSoftLight,		// 2*A*B+A2*(1-2*B) (B < 0.5) sqrt(A)*(2*b-1)+(2*a)*(1-b)
	blendDodge,			// A / (1-B)
	blendBurn,			// 1 - (1-A) / B
};


// Fixed Point.

// this is the maximum safe value for 32 bit integers
// so that (FP_ONE * FP_ONE) >> FP_BITS == FP_ONE.
let FP_ONE = 32768;
let FP_HALF = 16384;
let FP_BITS = 15;


// BlendSource.

struct BlendBuffer { // TODO align 128 (SSE2)
	RGBA16 p[8]; // 64 bytes.
};

struct BlendSource {
	void (*begin)(BlendSource* self,
				  int x, int y, int width, int height); // x,y >= 0 and w,h > 0.
	void (*next)(BlendSource* self); // advance to next line.
	void (*read8)(BlendSource* self, BlendBuffer* data); // read 8 pixels; 64 bytes.
	void (*read1)(BlendSource* self, RGBA16* data); // read one pixel.
	int width;
	int height; // TODO: these are in the way; only used by surface API.
};


// RGBA8 Blend functions.

typedef void (*blend_rgba8_func)(RGBA* dst, int len, BlendSource* src);

void blend_rgba8_copy(RGBA* dst, int len, BlendSource* src);
void blend_rgba8_normal(RGBA* dst, int len, BlendSource* src);
void blend_rgba8_add(RGBA* dst, int len, BlendSource* src);
void blend_rgba8_subtract(RGBA* dst, int len, BlendSource* src);


// RGBA16 Blend functions.

typedef void (*blend_rgba16_func)(RGBA16* dst, int len, BlendSource* src);


// Colour Fill.

void span4_col_copy(byte* dst, int len, RGBA col);
void span4_col_over(byte* dst, int len, RGBA col);
void span4_colp_over(byte* dst, int len, RGBA col);
void span4_colp_add(byte* dst, int len, RGBA col);

void span8_col_copy(RGBA16* dst, int len, RGBA16 col);
void span8_col_over(RGBA16* dst, int len, RGBA16 col);

//void span4_linear_copy(byte* dst, int len, RGBA c1, RGBA c2, int t0, int dt);
//void span4_linear_over(byte* dst, int len, RGBA c1, RGBA c2, int t0, int dt);
//void span4_linear_add(byte* dst, int len, RGBA c1, RGBA c2, int t0, int dt);


// Direct 1:1

void span4_4_copy(byte* dst, int len, byte* src, int alpha);

void span_add_rgba8(byte* from, byte* to, int len, int alpha);
void span_add_a_rgba8(byte* from, byte* to, int len, int alpha);
void span_add_rgba8(byte* from, byte* to, int len, int alpha);
void span_add_a_rgba8(byte* from, byte* to, int len, int alpha);
void span_over_rgba8_pre(byte* from, byte* to, int len, int alpha);
void span_over_rgba8(byte* from, byte* to, int len, int alpha);
void span_over_a_rgba8(byte* from, byte* to, int len, int alpha);


// Nearest Neighbour.

void span_copy_nn_rgba8(byte* from, byte* to, int len, int lerp, int inc);
void span_add_nn_rgba8(byte* from, byte* to, int len, int lerp, int inc);
void span_add_nn_a_rgba8(byte* from, byte* to, int len, int lerp, int inc, int alpha);


// RGB -> BGR (3 bytes swapped)

void span3s_col_copy(byte* dst, int len, RGBA col);
void span3s_col_over(byte* dst, int len, RGBA col);

void span3s_4_copy_nn(byte* dst, int len, byte* src, int lerp, int inc, int alpha);
void span3s_4p_over_nn(byte* dst, int len, byte* src, int lerp, int inc, int alpha);
void span3s_4p_over_nn_a(byte* dst, int len, byte* src, int lerp, int inc, int alpha);


// RGBA Colour.

export const RGBA rgba_black = { 0, 0, 0, 255 };
export const RGBA rgba_white = { 255, 255, 255, 255 };
export const RGBA rgba_grey = { 128, 128, 128, 255 };
export const RGBA rgba_transparent = { 0, 0, 0, 0 };


let IF16(X) = ((int_fast16_t)(X))
let UIF16(X) = ((uint_fast16_t)(X))
let UIF32(X) = ((uint_fast32_t)(X))

// NB. for A,B in [0,255] the uint_fast16_t operation ((A+1)*B)>>8 is
// idempotent when A=255 (i.e when alpha is 1.0)
// better would be: t=a*b+0x80, v=((t>>8)+t)>>8 [Blinn]
let U8MUL(A,B) = (((UIF16(A)+1)*(B))>>8)

// for A in [1,256] and B in [0,255].
let U8MUL_A1(A,B) = ((UIF16(A)*(B))>>8)

// for A,B in [0,65535]; requires uint_fast32_t.
let U16MUL(A,B) = (((UIF32(A)+1)*(B))>>16)
let U16MUL_A1(A,B) = ((UIF32(A)*(B))>>16)

// composite pre-multiplied B over pre-multiplied A.
let OVER_P(A,B,b) = ( (B) + U8MUL_A1(256-(b),(A)) )
let OVER16_P(A,B,b) = ( (B) + U16MUL_A1(65536-(b),(A)) )

// sum A and B with saturation.
let ADD_SAT(A,B) = {unsigned int t=(A); t+=(B); (A)=(t<=255)?t:255;}

// truncate 16-bit value by shifting to 8-bit.
let TR8(A) = ((A) >> 8)

// one minus a 16-bit value, plus one.
let OM16P(A) = (UIF32(65536)-(A))

// pre-multiplied 16-bit Aa over pre-multiplied 8-bit B.
// bits: tr8(A:16 + tr8(a:16 * B:8):16):8
let OVER_16P8(A,B,a) = TR8((A) + TR8(OM16P(a) * (B)))

// pre-multiplied 16-bit A plus pre-multiplied 8-bit B.
let ADD_16P8(B,A) = { uint_fast16_t t=UIF16(B); t+=TR8(A); (B)=(t<=255)?t:255; }
let SUB_16P8(B,A) = { int_fast16_t  t=IF16(B);  t-=TR8(A); (B)=(t>=0)?t:0; }


export RGBA rgba_premultiply(RGBA col)
{
	col.r = U8MUL(col.r, col.a);
	col.g = U8MUL(col.g, col.a);
	col.b = U8MUL(col.b, col.a);
	return col;
}

export RGBA16 rgba8_to_rgba16(RGBA col)
{
	RGBA16 c;
	c.r = UIF16(col.r) << 8;
	c.g = UIF16(col.g) << 8;
	c.b = UIF16(col.b) << 8;
	c.a = UIF16(col.a) << 8;
	return c;
}


// RGBA8 Blend functions.

void blend_rgba8_copy(RGBA* dst, int len, BlendSource* src)
{
	// "copy" blend mode, write source to destination.
	RGBA16 col;
	while (len--) {
		src->read1(src, &col);
		dst->r = TR8(col.r);
		dst->g = TR8(col.g);
		dst->b = TR8(col.b);
		dst->a = TR8(col.a);
		++dst;
	}
}

void blend_rgba8_normal(RGBA* dst, int len, BlendSource* src)
{
	// "normal" blend mode, both images pre-multiplied.
	RGBA16 col;
	while (len--) {
		src->read1(src, &col);
		dst->r = OVER_16P8(col.r, dst->r, col.a);
		dst->g = OVER_16P8(col.g, dst->g, col.a);
		dst->b = OVER_16P8(col.b, dst->b, col.a);
		dst->a = OVER_16P8(col.a, dst->a, col.a);
		++dst;
	}
}

void blend_rgba8_add(RGBA* dst, int len, BlendSource* src)
{
	RGBA16 col;
	while (len--) {
		src->read1(src, &col);
		ADD_16P8(dst->r, col.r);
		ADD_16P8(dst->g, col.g);
		ADD_16P8(dst->b, col.b);
		ADD_16P8(dst->a, col.a);
		++dst;
	}
}

void blend_rgba8_subtract(RGBA* dst, int len, BlendSource* src)
{
	RGBA16 col;
	while (len--) {
		src->read1(src, &col);
		SUB_16P8(dst->r, col.r);
		SUB_16P8(dst->g, col.g);
		SUB_16P8(dst->b, col.b);
		SUB_16P8(dst->a, col.a);
		++dst;
	}
}


// Colour Fill.

void span4_col_copy(byte* dst, int len, RGBA col)
{
	RGBA* to = (RGBA*)dst; // assumes align 4.
	while (len--) *to++ = col;
}

/*
void span3_fill(byte* dst, int len, RGBA col)
{
	if (len >= 12) {
		RGBA* to = (RGBA*)dst; // assumes align 4.
		RGBA c0, c1, c2;
		c0 = col; c0.a = col.r;
		c1.r = col.g; c1.g = col.b; c1.b = col.r; c1.a = col.g;
		c2.r = col.b; c2.g = col.r; c2.b = col.g; c2.a = col.b;
		while (len >= 3) {
			to[0] = c0;
			to[1] = c1;
			to[2] = c2;
			to += 3;
		}
	}
	while (len--) *to++ = col;
}
*/

void span4_col_over(byte* dst, int len, RGBA col)
{
	// pre-multiply the colour values.
	col.r = U8MUL(col.a, col.r);
	col.g = U8MUL(col.a, col.g);
	col.b = U8MUL(col.a, col.b);
	// premultiplied colour B over premultiplied destination A.
	while (len--) {
		// C' = B' + (1-b)A'
		dst[0] = OVER_P(dst[0], col.r, col.a);
		dst[1] = OVER_P(dst[1], col.g, col.a);
		dst[2] = OVER_P(dst[2], col.b, col.a);
		dst[3] = OVER_P(dst[3], col.a, col.a);
		dst += 4;
	}
}

void span4_colp_over(byte* dst, int len, RGBA col)
{
	// premultiplied colour B over premultiplied destination A.
	while (len--) {
		// C' = B' + (1-b)A'
		dst[0] = OVER_P(dst[0], col.r, col.a);
		dst[1] = OVER_P(dst[1], col.g, col.a);
		dst[2] = OVER_P(dst[2], col.b, col.a);
		dst[3] = OVER_P(dst[3], col.a, col.a);
		dst += 4;
	}
}

void span4_colp_add(byte* dst, int len, RGBA col)
{
	while (len--) {
		ADD_SAT(dst[0], col.r);
		ADD_SAT(dst[1], col.g);
		ADD_SAT(dst[2], col.b);
		ADD_SAT(dst[3], col.a);
		dst += 4;
	}
}

void span8_col_copy(RGBA16* dst, int len, RGBA16 col)
{
	while (len--) *dst++ = col;
}

void span8_col_over(RGBA16* dst, int len, RGBA16 col)
{
	// premultiplied colour B over premultiplied destination A.
	while (len--) {
		// C' = B' + (1-b)A'
		dst->r = OVER16_P(dst->r, col.r, col.a);
		dst->g = OVER16_P(dst->g, col.g, col.a);
		dst->b = OVER16_P(dst->b, col.b, col.a);
		dst->a = OVER16_P(dst->a, col.a, col.a);
		++dst;
	}
}


// Direct 1:1

void span4_4_copy(byte* dst, int len, byte* src, int alpha)
{
	memcpy(dst, src, (size_t)len << 2);
}

void span_add_rgba8(byte* from, byte* to, int len, int alpha)
{
	while (len--) {
		int r, g, b, a;
		r = to[0] + from[0]; to[0] = (r <= 255) ? r : 255;
		g = to[1] + from[1]; to[1] = (g <= 255) ? g : 255;
		b = to[2] + from[2]; to[2] = (b <= 255) ? b : 255;
		a = to[3] + from[3]; to[3] = (a <= 255) ? a : 255;
		to += 4;
		from += 4;
	}
}

void span_add_a_rgba8(byte* from, byte* to, int len, int alpha)
{
	while (len--) {
		int r, g, b, a;
		r = to[0] + ((from[0] * alpha) >> 8); to[0] = (r <= 255) ? r : 255;
		g = to[1] + ((from[1] * alpha) >> 8); to[1] = (g <= 255) ? g : 255;
		b = to[2] + ((from[2] * alpha) >> 8); to[2] = (b <= 255) ? b : 255;
		a = to[3] + ((from[3] * alpha) >> 8); to[3] = (a <= 255) ? a : 255;
		to += 4;
		from += 4;
	}
}

void span_over_rgba8_pre(byte* from, byte* to, int len, int alpha)
{
	// "normal" blend mode, both images pre-multiplied.
	while (len--) {
		int a, ia = 255 - from[3];
		to[0] = ((to[0] * ia) >> 8) + from[0];
		to[1] = ((to[1] * ia) >> 8) + from[1];
		to[2] = ((to[2] * ia) >> 8) + from[2];
		a = from[3] + to[3]; if (a>255) a=255; to[3] = a;
		from += 4;
		to += 4;
	}
}

void span_over_rgba8(byte* from, byte* to, int len, int alpha)
{
	// "normal" blend mode
	while (len--) {
		int a = from[3];
		int ia = 255 - a;
		to[0] = ((to[0] * ia) >> 8) + ((from[0] * a) >> 8);
		to[1] = ((to[1] * ia) >> 8) + ((from[1] * a) >> 8);
		to[2] = ((to[2] * ia) >> 8) + ((from[2] * a) >> 8);
		a += to[3]; if (a>255) a=255; to[3] = a;
		from += 4;
		to += 4;
	}
}

void span_over_a_rgba8(byte* from, byte* to, int len, int alpha)
{
	++alpha; // map 0..255 -> 1..256
	while (len--) {
		int a = (from[3] * alpha) >> 8;
		int ia = 255 - a;
		// TODO: source image should be premultiplied.
		to[0] = ((to[0] * ia) >> 8) + ((from[0] * a) >> 8);
		to[1] = ((to[1] * ia) >> 8) + ((from[1] * a) >> 8);
		to[2] = ((to[2] * ia) >> 8) + ((from[2] * a) >> 8);
		a += to[3]; if (a>255) a=255; to[3] = a;
		from += 4;
		to += 4;
	}
}


// Nearest Neighbour.

void span_copy_nn_rgba8(byte* from, byte* to, int len, int lerp, int inc)
{
	while (len--) {
		int j = (lerp >> FP_BITS) << 2;
		to[0] = from[j];
		to[1] = from[j+1];
		to[2] = from[j+2];
		to[3] = from[j+3];
		to += 4;
		lerp += inc;
	}
}

void span_add_nn_rgba8(byte* from, byte* to, int len, int lerp, int inc)
{
	while (len--) {
		int j = (lerp >> FP_BITS) << 2;
		int r, g, b, a;
		r = to[0] + from[j];   to[0] = (r <= 255) ? r : 255;
		g = to[1] + from[j+1]; to[1] = (g <= 255) ? g : 255;
		b = to[2] + from[j+2]; to[2] = (b <= 255) ? b : 255;
		a = to[3] + from[j+3]; to[3] = (a <= 255) ? a : 255;
		to += 4;
		lerp += inc;
	}
}

void span_add_nn_a_rgba8(byte* from, byte* to, int len, int lerp, int inc, int alpha)
{
	while (len--) {
		int j = (lerp >> FP_BITS) << 2;
		int r, g, b, a;
		r = to[0] + ((from[j]   * alpha) >> 8); to[0] = (r <= 255) ? r : 255;
		g = to[1] + ((from[j+1] * alpha) >> 8); to[1] = (g <= 255) ? g : 255;
		b = to[2] + ((from[j+2] * alpha) >> 8); to[2] = (b <= 255) ? b : 255;
		a = to[3] + ((from[j+3] * alpha) >> 8); to[3] = (a <= 255) ? a : 255;
		to += 4;
		lerp += inc;
	}
}


// RGBA -> BGR

void span3s_col_copy(byte* dst, int len, RGBA col)
{
	while (len--) {
		dst[0] = col.b;
		dst[1] = col.g;
		dst[2] = col.r;
		dst += 3;
	}
}

void span3s_col_over(byte* dst, int len, RGBA col)
{
	// pre-multiply the colour values.
	col.r = U8MUL(col.a, col.r);
	col.g = U8MUL(col.a, col.g);
	col.b = U8MUL(col.a, col.b);
	// premultiplied colour B over premultiplied destination A.
	while (len--) {
		// C' = B' + (1-b)A'
		dst[0] = OVER_P(dst[0], col.b, col.a);
		dst[1] = OVER_P(dst[1], col.g, col.a);
		dst[2] = OVER_P(dst[2], col.r, col.a);
		dst += 3;
	}
}

void span3s_4_copy_nn(byte* dst, int len, byte* src, int lerp, int inc, int alpha)
{
	while (len--) {
		int j = (lerp >> FP_BITS) << 2;
		dst[0] = src[j+2];
		dst[1] = src[j+1];
		dst[2] = src[j];
		dst += 3;
		lerp += inc;
	}
}

void span3s_4p_over_nn(byte* dst, int len, byte* src, int lerp, int inc, int alpha)
{
	while (len--) {
		int j = (lerp >> FP_BITS) << 2;
		// C' = B' + (1-b)A'
		int a = src[j+3];
		dst[0] = OVER_P(dst[0], src[j+2], a);
		dst[1] = OVER_P(dst[1], src[j+1], a);
		dst[2] = OVER_P(dst[2], src[j], a);
		dst += 3;
		lerp += inc;
	}
}

void span3s_4p_over_nn_a(byte* dst, int len, byte* src, int lerp, int inc, int alpha)
{
	// since the source is pre-multiplied, we need to multiply
	// source pixels by the blend alpha alone, and destination
	// pixels by one minus blend alpha times source alpha.
	++alpha; // map 0..255 -> 1..256
	while (len--) {
		int j = (lerp >> FP_BITS) << 2;
		int ia = 256 - U8MUL_A1(alpha, src[j+3]);
		dst[0] = U8MUL_A1(alpha, src[j+2]) + U8MUL_A1(ia, dst[0]);
		dst[1] = U8MUL_A1(alpha, src[j+1]) + U8MUL_A1(ia, dst[1]);
		dst[2] = U8MUL_A1(alpha, src[j+0]) + U8MUL_A1(ia, dst[2]);
		dst += 3;
		lerp += inc;
	}
}

/*
void span_sum_adj_a_rgba8_bgr8(byte* from, byte* to, int len, int lerp, int inc, int alpha)
{
	// merge adjacent source pixels (shrink or scale down)
	// NB. this generic version works when 0 < scale <= FP_ONE.
	int i = 0;
	int whole = (inc * alpha) >> FP_BITS;
	for (;;)
	{
		int next = lerp + inc;
		if (next < FP_ONE)
		{
			// whole source pixel is within dest pixel.
			to[i]   += (from[2] * whole) >> FP_BITS;
			to[i+1] += (from[1] * whole) >> FP_BITS;
			to[i+2] += (from[0] * whole) >> FP_BITS;
			lerp = next;
		}
		else
		{
			// source pixel spans two dest pixels.
			// accumulate the area within the left pixel.
			int amt = ((FP_ONE - lerp) * alpha) >> FP_BITS;
			to[i]   += (from[2] * amt) >> FP_BITS;
			to[i+1] += (from[1] * amt) >> FP_BITS;
			to[i+2] += (from[0] * amt) >> FP_BITS;
			lerp = next - FP_ONE;

			// advance to the next dest pixel.
			i += 3;
			if (i >= len)
				break; // finished.

			// accumulate the area within the right pixel.
			amt = (lerp * alpha) >> FP_BITS;
			to[i]   += (from[2] * amt) >> FP_BITS;
			to[i+1] += (from[1] * amt) >> FP_BITS;
			to[i+2] += (from[0] * amt) >> FP_BITS;
		}

		from += 4; // next source pixel.
	}
}
*/
