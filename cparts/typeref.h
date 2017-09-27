#ifndef CPART_TYPEREF
#define CPART_TYPEREF

typedef enum typeref_kind {
	typeref_kind_none = 0,
	typeref_kind_property,
	typeref_kind_group,
} typeref_kind;

typedef enum typeref_type {
	typeref_type_null = 0,
	typeref_type_bool,
	typeref_type_int,
	typeref_type_ipair,
	typeref_type_irect,
	typeref_type_str,
	typeref_type_obj,
} typeref_type;

// NB. ISO C void* cannot hold a function ptr.
typedef void (*any_func)();

// A structure with general alignment.
typedef union alignment_union { double d; int i; void* p; } alignment_union;
typedef struct any_struct { alignment_union align; } any_struct;

typedef union typeref_value {
	bool b;
	int i;
	iPair ipair;
	const char* str;
	any_struct* obj;
	any_func func;
	void* p;
	float f;
	double d;
} typeref_value;

typedef struct typeref_property {
	typeref_kind kind;
	typeref_type type;
	const char* name;
	any_func getter;
	any_func setter;
	const void* val;
} typeref_property;

// Property registration macros.
// Use these inside a static typeref_property array.
#define typeref_group(name, properties) { \
	typeref_kind_group, 0, (name), 0, 0, (properties) }
#define typeref_prop_bool(name, get, set, init) { \
	typeref_kind_property, typeref_type_bool, (name), (any_func)(get), (any_func)(set), (init) }
#define typeref_prop_int(name, set, get, init) { \
	typeref_kind_property, typeref_type_int, (name), (any_func)(get), (any_func)(set), (init) }
#define typeref_prop_ipair(name, set, get, init) { \
	typeref_kind_property, typeref_type_ipair, (name), (any_func)(get), (any_func)(set), (init) }
#define typeref_prop_irect(name, set, get, init) { \
	typeref_kind_property, typeref_type_irect, (name), (any_func)(get), (any_func)(set), (init) }
#define typeref_prop_str(name, set, get, init) { \
	typeref_kind_property, typeref_type_str, (name), (any_func)(get), (any_func)(set), (init) }
#define typeref_prop_obj(name, set, get) { \
	typeref_kind_property, typeref_type_obj, (name), (any_func)(get), (any_func)(set), (init) }

// Getter and setter prototypes for casting from any_func.

struct string;

typedef bool (*bool_getter)(any_struct* self);
typedef int (*int_getter)(any_struct* self);
typedef iPair (*ipair_getter)(any_struct* self);
typedef struct string* (*str_getter)(any_struct* self);
typedef any_struct* (*obj_getter)(any_struct* self);

typedef void (*bool_setter)(any_struct* self, bool val);
typedef void (*int_setter)(any_struct* self, int val);
typedef void (*ipair_setter)(any_struct* self, iPair val);
typedef void (*str_setter)(any_struct* self, struct string* val); // TODO: string and stringref.
typedef void (*obj_setter)(any_struct* self, any_struct* val);

// Misc.
typedef void (*init_func)(any_struct* self);
#define typeref_init_func(t, init) 

//typeref typeref_register(module m, const char* name, size_t size);


#endif
