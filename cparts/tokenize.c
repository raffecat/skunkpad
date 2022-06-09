// This was an idea that is not in use.

import * from str

// Tokenizer interface.

// Pass an interface to the tokenizer that it will use to create tokens.
// The tokenizer always returns a void pointer to the next token.

// Some choices for token backends:
// - use one union object to store all tokens; only valid until next().
// - allocate fixed-size tokens from a block allocator; attach strings.
// - allocate tokens with embedded strings.
// - allocate fixed-size tokens, include ref to source string object.

typedef size_t tokenPos;

typedef struct TokenContext TokenContext;
struct TokenContext {
	char* text;      // non-terminated token text.
	tokenPos length; // in bytes.
	tokenPos offset; // position in source text.
};

typedef struct TokenFactory_i TokenFactory_i, **TokenFactory;

struct TokenFactory_i {
	void* (*newline)(TokenFactory self, TokenContext* tc);
	void* (*whitespace)(TokenFactory self, TokenContext* tc);
	void* (*integer)(TokenFactory self, TokenContext* tc, long value);
	void* (*decimal)(TokenFactory self, TokenContext* tc, double value);
	void* (*symbol)(TokenFactory self, TokenContext* tc);
	void* (*ident)(TokenFactory self, TokenContext* tc);
};

// Some other choices for tokenizer design:

// Let next-token return an enum value similiar to the interface above;
// provide semantic getters to (lazy-convert) the token value.

// Let next-token return a TokenContext struct which is valid until
// the next token is gotten. Provide helpers to convert numeric forms
// from the token text to number values (extensible conversions.)

// Provide a stateful api with public "current token" enum and context.
// Provide helpers to convert numeric forms as above.
// Use the next() method to advance to the next token.

typedef int TokenValue;

typedef struct Tokenizer Tokenizer;
struct Tokenizer {
	void* data;       // user data.
	TokenValue token; // enumerated token.
	char* text;       // non-terminated token text.
	tokenPos length;  // in bytes.
	tokenPos offset;  // position in source text.
	void (*next)(Tokenizer* self);
};

// This will be called when an identifier is recognised.
// It should convert the current token into a keyword token.
typedef void (*TokenKeywordFunc)(Tokenizer* tokens);

// This should return true if the token matches an operator at its
// current length. The tokenizer will call again if more symbols
// follow; the longest match will be taken.
// Return enum { match, partial, prefix, nomatch } and stop when 'match'
// or 'nomatch' is returned; take the most recent 'match' or 'partial'.
// i.e. the longest valid operators 'match'; shorter valid operators
// 'partial' match; non-operator substrings 'prefix'.
// But this must do redundant lookups until a match is found..
typedef bool (*TokenSymbolFunc)(Tokenizer* tokens);

Tokenizer* createTokenizer(
	stringref source, // entire source text.
	TokenKeywordFunc keywordFunc, // keyword translation.
	TokenSymbolFunc symbolFunc // operator symbol translation.
);
