#ifndef CPART_STRING
#define CPART_STRING


// stringref - reference to (sub)string

typedef struct stringref { size_t size; const char* data; } stringref;

// stringref is null terminated for C library compatibility only when
// created with one of these macros, otherwise no such guarantee.
#define str_lit(cstr) { sizeof(cstr)/sizeof(char)-1, cstr }
#define str_fromc(cptr) { strlen((cptr)), (cptr) }
#define str_ref(s) { (s)->size, (s)->data }

// get a zero-terinated const char* from a strref.
#define strr_cstr(s) (s).data
#define strr_length(s) (s).size

extern const stringref str_empty;

bool str_equal(stringref s1, stringref s2);
stringref str_substr(stringref s, size_t first, size_t end);
size_t str_find(stringref s, stringref sub, size_t init);
size_t str_rfind(stringref s, stringref sub, size_t init);


// string
// internally null terminated for C library compatibility.

typedef struct string_s { size_t refs; size_t size; char data[1]; } *string;

// get a zero terminated character pointer for C library compatibility.
#define str_cstr(s) (s)->data

// get the size of the string contents in characters.
#define str_length(s) (s)->size

// increment the internal reference count for the string.
#define str_retain(s) (++(s)->refs)

// decrement the string reference count; destroy it when no refs remain.
#define str_release(s) (--(s)->refs || str_free_internal(s))

// create an empty string object; always a new string.
#define str_create_empty() str_alloc(0)

// get an empty string object, which might be shared.
#define str_empty() str_create_empty()

// allocate an uninitialised string, terminated at size.
string str_alloc(size_t size);

// create a string object containing the text from a stringref.
string str_create(stringref ref);

// create a string object from a zero-terminated C string.
string str_create_c(const char* cstr);

// create a string object from non-terminated character data of the given size.
string str_create_cl(const char* data, size_t size);

// create a string object from the concatenation of the text from two stringrefs.
string str_concat(stringref s1, stringref s2);


// stringbuf - string builder
// WARNING: do not access stringbuf members; the implementation will change.

// declare as automatic variable initialized with stringbuf_new.
typedef struct stringbuf { size_t alloc; string s; } stringbuf;

// initialise a buffer object, give initial buffer size hint.
#define stringbuf_new(size) { size, stringbuf_alloc_internal(size) }

// initialise a stringref from the buffer contents.
#define stringbuf_ref(buf) { (buf)->s->size, (buf)->s->data }

// append contents of stringref to the buffer.
void stringbuf_append(stringbuf* buf, stringref s);

// reserve space in the buffer for writing.
char* stringbuf_reserve(stringbuf* buf, size_t size);

// commit data written after stringbuf_reserve.
void stringbuf_commit(stringbuf* buf, size_t size);

// truncate the buffer contents at size.
void stringbuf_truncate(stringbuf* buf, size_t size);

// return buffer contents as a new string object, close the buffer.
string stringbuf_finish(stringbuf* buf);


// PRIVATE - not part of the public API.
string stringbuf_alloc_internal(size_t size);
int str_free_internal(string s);

#endif
