#include "defs.h"

#include <math.h>

typedef struct EvalNode EvalNode;
typedef struct EvalContext { int args; } EvalContext;
typedef void (*EvalFunc)(EvalContext* c, EvalNode* n);
struct EvalNode { EvalFunc eval; };

// integer nodes

typedef int EvalInt;
typedef struct iEvalNode iEvalNode;
typedef EvalInt (*iEvalFunc)(EvalContext* c, iEvalNode* n);
struct iEvalNode { iEvalFunc eval; };
typedef struct iBinOp { iEvalFunc eval; iEvalNode* a; iEvalNode* b; } iBinOp;
typedef struct iUnOp { iEvalFunc eval; iEvalNode* a; } iUnOp;

EvalInt iAdd(EvalContext* c, iBinOp* n) { return n->a->eval(c, n->a) + n->b->eval(c, n->b); }
EvalInt iSub(EvalContext* c, iBinOp* n) { return n->a->eval(c, n->a) - n->b->eval(c, n->b); }
EvalInt iMul(EvalContext* c, iBinOp* n) { return n->a->eval(c, n->a) * n->b->eval(c, n->b); }
EvalInt iDiv(EvalContext* c, iBinOp* n) { return n->a->eval(c, n->a) / n->b->eval(c, n->b); }
EvalInt iMod(EvalContext* c, iBinOp* n) { return n->a->eval(c, n->a) % n->b->eval(c, n->b); }
EvalInt iNeg(EvalContext* c, iUnOp* n) { return - n->a->eval(c, n->a); }
EvalInt iAbs(EvalContext* c, iUnOp* n) { EvalInt v = n->a->eval(c, n->a); return v<0?-v:v; }

// float nodes

typedef float EvalFloat;
typedef struct fEvalNode fEvalNode;
typedef EvalFloat (*fEvalFunc)(EvalContext* c, fEvalNode* n);
struct fEvalNode { fEvalFunc eval; };
typedef struct fBinOp { fEvalFunc eval; fEvalNode* a; fEvalNode* b; } fBinOp;
typedef struct fUnOp { fEvalFunc eval; fEvalNode* a; } fUnOp;

EvalFloat fAdd(EvalContext* c, fBinOp* n) { return n->a->eval(c, n->a) + n->b->eval(c, n->b); }
EvalFloat fSub(EvalContext* c, fBinOp* n) { return n->a->eval(c, n->a) - n->b->eval(c, n->b); }
EvalFloat fMul(EvalContext* c, fBinOp* n) { return n->a->eval(c, n->a) * n->b->eval(c, n->b); }
EvalFloat fDiv(EvalContext* c, fBinOp* n) { return n->a->eval(c, n->a) / n->b->eval(c, n->b); }
EvalFloat fMod(EvalContext* c, fBinOp* n) { return (EvalFloat)fmod(n->a->eval(c, n->a), n->b->eval(c, n->b)); }
EvalFloat fNeg(EvalContext* c, fUnOp* n) { return - n->a->eval(c, n->a); }
EvalFloat fAbs(EvalContext* c, fUnOp* n) { EvalFloat v = n->a->eval(c, n->a); return v<0?-v:v; }

// conversion

EvalFloat fIntToFloat(EvalContext* c, iEvalNode* n) { return (EvalFloat)n->eval(c, n); }
EvalInt fFloatToInt(EvalContext* c, fEvalNode* n) { return (EvalInt)n->eval(c, n); }
