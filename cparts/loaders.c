#include "defs.h"
#include "loaders.h"


// FileLoader

typedef struct FileLoader { LOADER_FIELDS; string path; } FileLoader;

static void fl_destroy(Object* o) {
	FileLoader* l = (FileLoader*)o;
	//release(l->path);
	cpart_free(o);
}

static ObjectType type_file_loader = { "FileLoader", fl_destroy };

static Resource* fl_load(Loader* loader, ResourceId* id) {
	FileLoader* l = (FileLoader*)loader;
	return 0;
}

Loader* create_file_loader(string path) {
	FileLoader* l = cpart_new(FileLoader);
	l->type = &type_file_loader;
	l->load = fl_load;
	l->path = path; //retain(path);
	return (Loader*)l;
}
