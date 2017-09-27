#ifndef CPART_REFS
#define CPART_REFS

// Reference counted object and smart pointers.

#include <assert.h>

// object base class implementing reference count.
class object {
public:
	inline object() : refs(0) {}
	inline void retain() { assert(refs >= 0); ++refs; }
	inline void release() { if (!--refs) { --refs; delete this; } }
private:
	long refs;
};

// ownership of heap object; will delete when destroyed.
template<class T> class own {
public:
	inline own(T* p=0) throw() : o(p) {}
	inline own(own& r) throw() : o(r.detach()) {}
	inline own& operator=(own& r) throw() { if (o) delete o; o=r.detach(); return *this; }
	inline own& operator=(T* p) throw() { if (o) delete o; o=p; return *this; }
	inline T* get() const throw() { return o; }
	inline T* detach() throw() { T* p=o; o=0; return p; }
	inline T& operator*() const throw() { assert(o != 0); return *o; }
	inline T* operator->() const throw() { assert(o != 0); return o; }
	inline operator T* () throw() { return detach(); }
	inline ~own() throw() { if (o) delete o; o=0; }
private: T* o;
};

// strong reference; will retain on assign, release when destroyed.
template<class T> class ref {
public:
	inline ref() throw() : o(0) {}
	inline ref(T* p) throw() : o(p) { p->retain(); }
	inline ref(ref& r) throw() : o(r) { o->retain(); }
	inline ref& operator=(ref& r) throw() { T* n = r; if (n) n->retain(); if (o) o->release(); o=n; return *this; }
	inline ref& operator=(T* p) throw() { T* n = p; if (n) n->retain(); if (o) o->release(); o=n; return *this; }
	inline T* get() const throw() { return o; }
	inline T& operator*() const throw() { assert(o != NULL); return *o; }
	inline T* operator->() const throw() { assert(o != NULL); return o; }
	inline operator T* () const throw() { return o; }
	inline ~ref() throw() { if (o) o->release(); o=0; }
private: T* o;
};

#endif
