#ifndef CPART_LOADERS
#define CPART_LOADERS


#include "str.h"


// ReadStream - iterator for stream reading.

// * call next() for more data until it returns false.
// * size and data can be modified; both are reset on next().
// * must call close() unless next() has returned false.

typedef struct ReadStream ReadStream; struct ReadStream {
	size_t size; byte* data;
	bool (*next)(ReadStream* stream);
	void (*close)(ReadStream* stream);
	struct ReadStreamPrivate { void* src; byte* buf; size_t pos; } _priv;
};


// Resource - data resource.

typedef struct Resource Resource;

typedef struct ResourceType {
	size_t refs;
} ResourceType;

struct Resource {
	ResourceType rt;
	bool (*read)(Resource* self, ReadStream* s);
};


// Loader - resource loader.

typedef struct Loader Loader;
typedef struct ResourceId { const char* type; } ResourceId;

struct Loader {
	size_t refs;
	Resource* (*load)(Loader* loader, ResourceId* id);
};


// FileLoader

Loader* create_file_loader(string path);


#endif
