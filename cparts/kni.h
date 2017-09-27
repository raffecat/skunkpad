//--------------------------------------------------------------------------
// Rob Wyatt for Game Developer/Gamasutra April 1999
//--------------------------------------------------------------------------

#ifndef KNI_HEADER
#define KNI_HEADER

// SIMD Registers (with Mod/RM register prefix)
#define _XMM0 (0xc0)
#define _XMM1 (0xc1)
#define _XMM2 (0xc2)
#define _XMM3 (0xc3)
#define _XMM4 (0xc4)
#define _XMM5 (0xc5)
#define _XMM6 (0xc6)
#define _XMM7 (0xc7)

// MMX Registers (with Mod/RM register prefix)
#define _MM0 (0xc0)
#define _MM1 (0xc1)
#define _MM2 (0xc2)
#define _MM3 (0xc3)
#define _MM4 (0xc4)
#define _MM5 (0xc5)
#define _MM6 (0xc6)
#define _MM7 (0xc7)

// Integer registers used as address pointers
#define	EAX_PTR (0)
#define	EBX_PTR (3)
#define	ECX_PTR (1)
#define	EDX_PTR (2)
#define	ESI_PTR (6)
#define	EDI_PTR (7)
#define	EBP_PTR (5)
#define	ESP_PTR (4)

// actual integer registers
#define	EAX_REG (0xc0)
#define	EBX_REG (0xc3)
#define	ECX_REG (0xc1)
#define	EDX_REG (0xc2)
#define	ESI_REG (0xc6)
#define	EDI_REG (0xc7)
#define	EBP_REG (0xc5)
#define	ESP_REG (0xc4)

#define _EQ			(0)
#define _LT			(1)
#define _LE			(2)
#define _UNORDERED	(3)
#define _NE			(4)
#define _NEQ		(4)
#define _GE			(5)
#define _NLT		(5)
#define _GT			(6)
#define _NLE		(6)
#define _ORDERED	(7)
// Some better names for ordered and unordered
#define _QNAN		(3) // true if one of the inputs is a QNAN
#define _NUM		(7)	// false if one of the inputs is a QNAN

// Store 128bits to the effective address in dst fron the
// specified SIMD src register.
// NOTE: This is a dst memory form of the MOVAPS instruction.
#define STOREA(dst, src)					\
{											\
	_asm _emit 0x0f							\
	_asm _emit 0x29							\
	_asm _emit ((src & 0x3f)<<3) | (dst)	\
}

#define MOVAPS(dst, src)					\
{											\
	_asm _emit 0x0f							\
	_asm _emit 0x28							\
	_asm _emit ((dst & 0x3f)<<3) | (src)	\
}

#define STOREU(dst, src)					\
{											\
	_asm _emit 0x0f							\
	_asm _emit 0x11							\
	_asm _emit ((src & 0x3f)<<3) | (dst)	\
}

#define MOVUPS(dst, src)					\
{											\
	_asm _emit 0x0f							\
	_asm _emit 0x10							\
	_asm _emit ((dst & 0x3f)<<3) | (src)	\
}

#define ADDPS(dst, src)						\
{											\
	_asm _emit 0x0f							\
	_asm _emit 0x58							\
	_asm _emit ((dst & 0x3f)<<3) | (src)	\
}

#define ADDSS(dst, src)						\
{											\
	_asm _emit 0xf3							\
	_asm _emit 0x0f							\
	_asm _emit 0x58							\
	_asm _emit ((dst & 0x3f)<<3) | (src)	\
}


#define DIVPS(dst, src)						\
{											\
	_asm _emit 0x0f							\
	_asm _emit 0x5e							\
	_asm _emit ((dst & 0x3f)<<3) | (src)	\
}

#define DIVSS(dst, src)						\
{											\
	_asm _emit 0xf3							\
	_asm _emit 0x0f							\
	_asm _emit 0x5e							\
	_asm _emit ((dst & 0x3f)<<3) | (src)	\
}

#define ANDNPS(dst, src)					\
{											\
	_asm _emit 0x0f							\
	_asm _emit 0x55							\
	_asm _emit ((dst & 0x3f)<<3) | (src)	\
}

#define ANDPS(dst, src)						\
{											\
	_asm _emit 0x0f							\
	_asm _emit 0x54							\
	_asm _emit ((dst & 0x3f)<<3) | (src)	\
}

#define XORPS(dst, src) 					\
{											\
	_asm _emit 0x0f							\
	_asm _emit 0x57							\
	_asm _emit ((dst & 0x3f)<<3) | (src)	\
}

#define CMPPS(dst, src, cond)				\
{											\
	_asm _emit 0x0f							\
	_asm _emit 0xC2							\
	_asm _emit ((dst & 0x3f)<<3) | (src)	\
	_asm _emit cond							\
}

#define CMPEQPS(dst,src) CMPPS(dst,src,_EQ)
#define CMPLTPS(dst,src) CMPPS(dst,src,_LT)
#define CMPLEPS(dst,src) CMPPS(dst,src,_LE)
#define CMPUNORDPS(dst,src) CMPPS(dst,src,_UNORDERED)
#define CMPNEQPS(dst,src) CMPPS(dst,src,_NEQ)
#define CMPNEPS(dst,src) CMPPS(dst,src,_NEQ)
#define CMPNLTPS(dst,src) CMPPS(dst,src,_NLT)
#define CMPGEPS(dst,src) CMPPS(dst,src,_NLT)
#define CMPNLEPS(dst,src) CMPPS(dst,src,_NLE)
#define CMPGTPS(dst,src) CMPPS(dst,src,_NLE)
#define CMPORDPS(dst,src) CMPPS(dst,src,_ORDERED)

#define CMPSS(dst, src, cond)				\
{											\
	_asm _eint 0xf3							\
	_asm _emit 0x0f							\
	_asm _emit 0xC2							\
	_asm _emit ((dst & 0x3f)<<3) | (src)	\
	_asm _emit cond							\
}

#define CMPEQSS(dst,src) CMPSS(dst,src,_EQ)
#define CMPLTSS(dst,src) CMPSS(dst,src,_LT)
#define CMPLESS(dst,src) CMPSS(dst,src,_LE)
#define CMPUNORDSS(dst,src) CMPSS(dst,src,_UNORDERED)
#define CMPNEQSS(dst,src) CMPSS(dst,src,_NEQ)
#define CMPNESS(dst,src) CMPSS(dst,src,_NEQ)
#define CMPNLTSS(dst,src) CMPSS(dst,src,_NLT)
#define CMPGESS(dst,src) CMPSS(dst,src,_NLT)
#define CMPNLESS(dst,src) CMPSS(dst,src,_NLE)
#define CMPGTSS(dst,src) CMPSS(dst,src,_NLE)
#define CMPORDSS(dst,src) CMPSS(dst,src,_ORDERED)

#define COMISS(dst, src)					\
{											\
	_asm _emit 0x0f							\
	_asm _emit 0x2f							\
	_asm _emit ((dst & 0x3f)<<3) | (src)	\
}

#define CVTPI2PS(dst, src)					\
{											\
	_asm _emit 0x0f							\
	_asm _emit 0x2a							\
	_asm _emit ((dst & 0x3f)<<3) | (src)	\
}

#define CVTPS2PI(dst, src)					\
{											\
	_asm _emit 0x0f							\
	_asm _emit 0x2d							\
	_asm _emit ((dst & 0x3f)<<3) | (src)	\
}

#define CVTSI2SS(dst, src)					\
{											\
	_asm _emit 0xf3							\
	_asm _emit 0x0f							\
	_asm _emit 0x2a							\
	_asm _emit ((dst & 0x3f)<<3) | (src)	\
}

#define CVTSS2SI(dst, src)					\
{											\
	_asm _emit 0xf3							\
	_asm _emit 0x0f							\
	_asm _emit 0x2d							\
	_asm _emit ((dst & 0x3f)<<3) | (src)	\
}

#define LDMXCSR      					    \
{											\
    _asm _emit 0x0f                         \
    _asm _emit 0xae                         \
    _asm _emit 0x55                         \
    _asm _emit 0x00                         \
}

#define STMXCSR      					    \
{											\
    _asm _emit 0x0f                         \
    _asm _emit 0xae                         \
    _asm _emit 0x5d                         \
    _asm _emit 0x00                         \
}

#endif
