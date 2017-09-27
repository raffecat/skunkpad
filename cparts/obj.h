#ifndef CPART_OBJ
#define CPART_OBJ

#error included obj.h

/*
// Object - reference counted object.

typedef struct Object Object;

#define OBJECT_TYPE_FIELDS \
	const char* name; \
	void (*destroy)(Object* self)

typedef struct ObjectType { OBJECT_TYPE_FIELDS; } ObjectType;

#define OBJECT_FIELDS ObjectType* type; int refs
#define OBJECT_SUBTYPE(T) T* type; int refs
struct Object { OBJECT_FIELDS; };

#define ObjRetain(OBJ) ( ++(OBJ)->refs, (OBJ) )
#define ObjRelease(OBJ) do { if (!--(OBJ)->refs) { (OBJ)->type->destroy((Object*)(OBJ)); } } while(0)

// use this on new objects when finished with them.
// if they have been retained somewhere, this will do nothing.
#define ObjCleanup(OBJ) do { if (!(OBJ)->refs) { (OBJ)->type->destroy((Object*)(OBJ)); } } while(0)
*/

#endif
