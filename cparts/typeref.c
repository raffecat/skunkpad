#include "defs.h"
#include "typeref.h"

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

typedef struct mod_item mod_item;
typedef struct typeref_property typeref_property;

typedef struct typeref *typeref;
typedef struct module *module;

struct typeref {
	string name;
	size_t size;
	int members;
	typeref_property* mem;
	int mem_alloc;
	init_func init;
};

struct mod_item {
	struct typeref tr;
	mod_item* next;
};

struct module {
	mod_item* items;
};

static struct string g_nullname = { 0, { 0 } };
static struct typeref g_nulltype = { &g_nullname, 0 };
static struct typeref_property g_nullmem = { 0 };
//static struct any_struct* g_null = { &g_nulltype };

/*
module create_module()
{
	module m = cpart_new(struct module);
	m->items = 0;
	return m;
}

typeref typeref_register(module m, const char* name, size_t size)
{
	mod_item* i = cpart_new(struct mod_item);
	typeref tr = &i->tr;
	i->next = m->items; m->items = i; // insert.
	tr->name = str_create_c(name);
	tr->size = size;
	tr->members = 0; tr->mem = 0; tr->mem_alloc = 0;
	tr->init = 0;
	return tr;
}

void typeref_add_prop(typeref t, type_enum fmt, const char* name, any_func set, any_func get)
{
	typeref_property* tm;
	if (t->members == t->mem_alloc) {
		t->mem_alloc += 8;
		t->mem = (typeref_property*)realloc(t->mem, sizeof(typeref_property) * t->mem_alloc);
	}
	tm = t->mem + t->members;
	t->members += 1;
	tm->fmt = fmt;
	tm->get = get;
	tm->set = set;
	tm->name = str_create_c(name);
}

void typeref_init_func_(typeref t, init_func init) {
	t->init = init;
}

typeref mod_get_type(module m, stringref name)
{
	mod_item* i = m->items;
	while (i) {
		stringref s = str_ref(i->tr.name);
		if (str_equal(s, name)) {
			return &i->tr;
		}
		i = i->next;
	}
	return &g_nulltype;
}

any_struct* create(module m, const char* name)
{
	stringref nom = str_fromc(name);
	typeref t = mod_get_type(m, nom);
	any_struct* o = cpart_alloc(t->size);
	cpart_zero(o, t->size);
	o->type = t;
	if (t->init) t->init(o);
	return o;
}

typeref_property* typeref_find_mem(typeref t, const char* name)
{
	stringref nom = str_fromc(name);
	int i, n = t->members; typeref_property* m = t->mem;
	for (i=0; i<n; ++i) {
		stringref s = str_ref(m[i].name);
		if (str_equal(s, nom))
			return &m[i];
	}
	return &g_nullmem;
}

void set_bool(any_struct* o, const char* name, bool val)
{
	typeref_property* m = typeref_find_mem(o->type, name);
	if (m->set) m->set(o, val);
}

void set_int(any_struct* o, const char* name, int val)
{
	typeref_property* m = typeref_find_mem(o->type, name);
	if (m->set) m->set(o, val);
}

void set_ipairv(any_struct* o, const char* name, int x, int y)
{
	ipair val = { x, y };
	typeref_property* m = typeref_find_mem(o->type, name);
	if (m->set) m->set(o, val);
}

void set_strc(any_struct* o, const char* name, const char* val)
{
	stringref sref = str_fromc(val);
	typeref_property* m = typeref_find_mem(o->type, name);
	if (m->set) m->set(o, sref);
}

void set_obj(any_struct* o, const char* name, any_struct* val)
{
	typeref_property* m = typeref_find_mem(o->type, name);
	if (m->set) m->set(o, val);
}
*/
