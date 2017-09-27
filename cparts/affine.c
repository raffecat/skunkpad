/* Copyright (c) 2010, Andrew Towers <atowers@gmail.com>
** All rights reserved.
*/

// 2x3 affine transformation matrix:

// x'   [ Ux, Vx, Tx ] [ x ]
// y' = [ Uy, Vy, Ty ] [ y ]
// 1    [  0,  0,  1 ] [ 1 ] (not stored)

struct Affine2D {
	float Ux, Uy; // Column 1: X basis (x,y)
	float Vx, Vy; // Column 2: Y basis (x,y)
	float Tx, Ty; // Column 3: translation
}

let Affine2D = type const ref Affine2D;
let Affine2D_out = type ref Affine2D;

export const identity = Affine2D(
	1.0, 0.0, // Column 1: X basis (x,y)
	0.0, 1.0, // Column 2: Y basis (x,y)
	0.0, 0.0  // Column 3: translation
);

// in C we must define functions for efficient in-place operations
// as well as functions for the general case (from, to) structures.
// ... because the compiler cannot vary the implementation to perform
//     in-place updates where safe to do so.

def affine_set_identity() Affine2D result {
	result.Ux = 1.0f;
	result.Vy = 1.0f;
	result.Vx = 0.0f;
	result.Tx = 0.0f;
	result.Uy = 0.0f;
	result.Ty = 0.0f;
}


// set a translation-only affine transform.
def affine_translate(float x, float y) Affine2D result {
	result.Ux = 1.0f;
	result.Vy = 1.0f;
	result.Vx = 0.0f;
	result.Uy = 0.0f;
	result.Tx = x;
	result.Ty = y;
}


// set a scale-only affine transform.
def affine_scale(float sx, float sy) Affine2D result
{
	result.Ux = sx;
	result.Vy = sy;
	result.Vx = 0.0f;
	result.Uy = 0.0f;
	result.Tx = 0.0f;
	result.Ty = 0.0f;
}


// multiply the left affine matrix by the right (concatenate transforms)
// NB. result cannot alias left or right; left can alias right.
Affine2D affine_multiply(Affine2D left, Affine2D right)
{
	// 12 multiply, 8 add (2x2 . U,V and 2x3 . Origin)
	//assert(result != left && result != right);
	result.Ux = left.Ux * right.Ux + left.Vx * right.Uy;
	result.Vx = left.Ux * right.Vx + left.Vx * right.Vy;
	result.Tx = left.Ux * right.Tx + left.Vx * right.Ty + left.Tx;
	result.Uy = left.Uy * right.Ux + left.Vy * right.Uy;
	result.Vy = left.Uy * right.Vx + left.Vy * right.Vy;
	result.Ty = left.Uy * right.Tx + left.Vy * right.Ty + left.Ty;
}


// in C this must be a batch operation that takes arrays.
// ideally define it for one element and generate stream/array batch code.
// ... this applies to any function used in stream/array context.

// transform an array of x,y pairs.
// transform points to 6 floats; src and dest point to 2*count floats.
// NB. src can alias dest.

// x'   [ Ux, Vx, Tx ] [ x ]
// y' = [ Uy, Vy, Ty ] [ y ]
// 1    [  0,  0,  1 ] [ 1 ]

void affine_transform(Affine2D t, float* src, float* dest, int count)
{
	while (count--) {
		float x = src[0], y = src[1]; // allow dest to alias src.
		dest[0] = x * t.Ux + y * t.Vx + t.Tx;
		dest[1] = x * t.Uy + y * t.Vy + t.Ty;
		dest += 2; src += 2;
	}
}


// untransform (inverse transform) an array of x,y pairs.
// transform points to 6 floats; src and dest point to 2*count floats.
// NB. src can alias dest.

// A = [ M   b  ]
//     [ 0   1  ]
//
// inv(A) = [ inv(M)   -inv(M) * b ]
//          [   0            1     ]
//
// inv(A) * [x] = [ inv(M) * (x - b) ]
//          .Vx = [        1         ] 
//
// if M represents a rotation (columns are orthonormal) then inv(M) = transpose(M)
// http://stackoverflow.com/questions/2624422/efficient-4x4-matrix-inverse-affine-transform

// [ ScaleX = Ux, SkewX  = Vx, TransX = Tx ]
// [ SkewY  = Uy, ScaleY = Vy, TransY = Ty ]

void affine_transform_inv(Affine2D t, float* src, float* dest, int count)
{
	// compute inverse 2x2 linear transform.
	// TODO: can just transpose if U & V are colinear.
	float rd = 1.0f / (t.Ux * t.Vy - t.Vx * t.Uy); // reciprocal determinant.
	float iUx =  t.Vy * rd;
	float iVx = -t.Vx * rd;
	float iUy = -t.Uy * rd;
	float iVy =  t.Ux * rd;
	// apply inverse transform to all points.
	while (count--) {
		// translate before linear transform.
		float x = src[0] - t.Tx, y = src[1] - t.Ty;
		// now apply inverse 2x2 matrix.
		dest[0] = x * iUx + y * iVx;
		dest[1] = x * iUy + y * iVy;
		dest += 2; src += 2;
	}
}


// multiply an x,y translation matrix by the transform.
// transform points to 6 floats; result points to 6 uninitialised floats.
// NB. result can alias transform.

Affine2D affine_pre_translate(Affine2D t, float x, float y)
{
	result.Ux = t.Ux;
	result.Uy = t.Uy;
	result.Vx = t.Vx;
	result.Vy = t.Vy;
	result.Tx = t.Tx + x * t.Ux + y * t.Vx;
	result.Ty = t.Ty + x * t.Uy + y * t.Vy;
}


// multiply the transform by an x,y translation matrix.
// transform points to 6 floats; result points to 6 uninitialised floats.
// NB. result can alias transform.

Affine2D affine_post_translate(Affine2D t, float x, float y)
{
	result.Ux = t.Ux;
	result.Uy = t.Uy;
	result.Vx = t.Vx;
	result.Vy = t.Vy;
	result.Tx = t.Tx + x;
	result.Ty = t.Ty + y;
}


// multiply a scale matrix by the transform.
// transform points to 6 floats; result points to 6 uninitialised floats.
// NB. result can alias transform.

Affine2D affine_pre_scale(Affine2D t, float sx, float sy)
{
	result.Ux = t.Ux * sx;
	result.Uy = t.Uy * sy;
	result.Vx = t.Vx * sx;
	result.Vy = t.Vy * sy;
	result.Tx = t.Tx * sx;
	result.Ty = t.Ty * sy;
}


// multiply the transform by a scale matrix.
// transform points to 6 floats; result points to 6 uninitialised floats.
// NB. result can alias transform.

Affine2D affine_post_scale(Affine2D t, float sx, float sy)
{
	// disagrees with http://www.senocular.com/flash/tutorials/transformmatrix/
	// but came from affine_multiply by removing zero terms...
	result.Ux = t.Ux * sx;
	result.Uy = t.Uy * sx;
	result.Vx = t.Vx * sy;
	result.Vy = t.Vy * sy;
	result.Tx = t.Tx;
	result.Ty = t.Ty;
}
