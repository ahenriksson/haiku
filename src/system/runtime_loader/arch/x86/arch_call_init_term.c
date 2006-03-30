/*
 * Copyright 2006, Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Axel Dörfler, axeld@pinc-software.de
 *		Ingo Weinhold, bonefish@cs.tu-berlin.de
 */ 


#include "runtime_loader_private.h"


void
arch_call_init(image_t *image)
{
	// This calls the init code in crti.S (in glue/arch/x86/)
	// It would have been too easy to just call this function with its arguments
	// on the stack, so Be went the ugly way. Once more.
	asm("push	%%ebx;"
		"mov	%0, %%ebx;"
		"call	*%1;"
		"pop	%%ebx"
		: : "g"(image->id), "g"(image->init_routine));
}


void
arch_call_term(image_t *image)
{
	asm("push	%%ebx;"
		"mov	%0, %%ebx;"
		"call	*%1;"
		"pop	%%ebx"
		: : "g"(image->id), "g"(image->term_routine));
}

