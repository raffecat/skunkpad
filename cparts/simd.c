#include "defs.h"
#include "KNI.h"

#include <windows.h> // EXCEPTION_EXECUTE_HANDLER

export int cpu_supports_simd()
{
	int has_support = 0;

	__try {
		_asm {
			pushfd
			pop eax				// get EFLAGS
			mov ebx, eax		// keep a copy
			xor eax, 0x200000	// toggle CPUID bit

			push eax
			popfd				// set new EFLAGS
			pushfd
			pop eax				// get EFLAGS again

			xor eax,ebx			// have we changed the ID bit?
			je no_support		// -> no CPUID instruction

			mov eax, 1
			cpuid
			test edx, (1<<25)
			jz no_support		// -> no SIMD support

			XORPS(_XMM0,_XMM0)	// check OS support for SIMD

			mov has_support, 1	// success!
		}
	} __except(EXCEPTION_EXECUTE_HANDLER) {
		// XORPS throws STATUS_ILLEGAL_INSTRUCTION if no OS support.
	}
no_support:

	return has_support;
}

// __declspec(align(16)) float array[ARRAY_SIZE];
// (float*) _aligned_malloc(ARRAY_SIZE * sizeof(float), 16);
