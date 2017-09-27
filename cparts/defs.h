#ifndef CPART_DEFS
#define CPART_DEFS

#include <stdlib.h> // malloc, free, size_t, NULL
#include <stddef.h> // offsetof
#include <string.h> // memset, memcpy, etc
#include <assert.h> // assert

#ifdef _WIN32
#define WINDOWS
#define CDECL __cdecl
#else
#define CDECL
#endif

// Duly copied and pasted as directed by Tom Forsyth.
// Well, except that his names are all completely wrong.
// TODO: start using Sean's sophist.h as well.
typedef signed char int8;
typedef unsigned char byte;
typedef signed short int16;
typedef unsigned short uint16;
typedef signed long int32;
typedef unsigned long uint32;
typedef signed __int64 int64;
typedef unsigned __int64 uint64;

typedef int int_fast8_t;
typedef int int_fast16_t;
typedef int int_fast32_t;
typedef unsigned int uint_fast8_t;
typedef unsigned int uint_fast16_t;
typedef unsigned int uint_fast32_t;

#ifndef __cplusplus
typedef enum bool { false=0, true=1 } bool;
#endif

typedef struct dPair { double x, y; } dPair;
typedef struct dRect { double left, top, right, bottom; } dRect;
typedef struct Transform { dPair org; double scale; } Transform;

typedef struct iPair { int x, y; } iPair;
typedef struct iRect { int left, top, right, bottom; } iRect;

typedef struct fPair { float x, y; } fPair;
typedef struct fRect { float left, top, right, bottom; } fRect;
typedef struct fRGBA { float red, green, blue, alpha; } fRGBA;

typedef struct RGBA { byte r,g,b,a; } RGBA;
typedef struct RGBA16 { uint16 r,g,b,a; } RGBA16;

// TODO: move to transform lib?
dPair transform(Transform* t, dPair pt);
dPair untransform(Transform* t, double x, double y);

#ifndef __cplusplus

#define cpart_new(STRUCT) calloc(1, sizeof(struct STRUCT))
#define cpart_alloc(SIZE) malloc((SIZE))
#define cpart_zero(PTR, SIZE) memset((PTR), 0, (SIZE))
#define cpart_free(PTR) free((PTR))

// find an object from a field pointer within the object.
#define FPtoSelf(STRUCT, FIELD, PTR) ((STRUCT*)( (char*)(PTR) - offsetof(STRUCT, FIELD) ))
#define FPtoObj(STRUCT, FIELD, PTR) ((Object*)( (char*)(PTR) - offsetof(STRUCT, FIELD) ))

// interfaces.
#define impl_cast(IMPL,FIELD,PTR) ((IMPL*)( (char*)(PTR)-offsetof(IMPL,FIELD) ))

// generic interface reference counting,
// where an interface handle is a pointer to a pointer to a function table.
#define retain(P) ++(*(size_t*)( (byte*)(P) + (*(P))->_ref_disp ))
#define release(P) do { if (!--(*(size_t*)( (byte*)(P) + (*(P))->_ref_disp ))) (*(P))->_destruct((P)); } while(0)

#define ALIGN(X) __declspec(align(X))
void CDECL trace(const char* fmt, ...);
void memtest();

#endif // __cplusplus

typedef struct dataBuf { byte* data; size_t size;
						 void (*free)(struct dataBuf* buf); } dataBuf;

#include "str.h"

/*
#include "typeref.h"

module create_module();
object create(module m, const char* name);

void set_bool(object o, const char* name, bool val);
void set_int(object o, const char* name, int val);
void set_ipairv(object o, const char* name, int x, int y);
void set_strc(object o, const char* name, const char* val);
void set_obj(object o, const char* name, object val);
*/

#endif
