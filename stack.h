/* Copyright (c) 2007-2012 IETF Trust, CSIRO, Xiph.Org Foundation,
                           Gregory Maxwell. All rights reserved.
   Written by Jean-Marc Valin and Gregory Maxwell */
/*

   This file is extracted from RFC6716. Please see that RFC for additional
   information.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

   - Neither the name of Internet Society, IETF or IETF Trust, nor the
   names of specific contributors, may be used to endorse or promote
   products derived from this software without specific prior written
   permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
   OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef STACK_H
#define STACK_H

#include <stdlib.h>
#include <stddef.h>

#define GLOBAL_STACK_SIZE 100000

extern char *global_stack;

#ifndef OVERRIDE_OPUS_ALLOC_SCRATCH
static inline void *opus_alloc_scratch (size_t size)
{
   /* Scratch space doesn't need to be cleared */
   return malloc(size);
}
#endif


#define ALIGN(stack, size) ((stack) += ((size) - (long)(stack)) & ((size) - 1)) //uses bitwise & to calculate padding to align to required size
#define PUSH(stack, size, type) (ALIGN((stack),sizeof(type)/sizeof(char)),(stack)+=(size)*(sizeof(type)/sizeof(char)),(type*)((stack)-(size)*(sizeof(type)/sizeof(char)))) //aligns stack to proper boundary, then sets stack pointer(allocating the mem) to the proper distance (essentially how many elements of this type), then returning a pointer to where the new memory started its allocation
#define RESTORE_STACK (global_stack = _saved_stack) //resetting global to where it was last saved, start of stack (0) by default
#define ALLOC_STACK char *_saved_stack; (global_stack = (global_stack==0) ? opus_alloc_scratch(GLOBAL_STACK_SIZE) : global_stack); _saved_stack = global_stack; //checks that stack is at its start of 0, then mallocs a constant (10000 I think it was) setting that pointer to the global stack var, then sets saved stack to that start as well.

#define VARDECL(type, var) type *var //pretty unnecessary, I think it is just trying to make it clear what vars are going onto the custom stack
#define ALLOC(var, size, type) var = PUSH(global_stack, size, type) //simply sets var to the pointer at start of memory section PUSH allocated
#define SAVE_STACK char *_saved_stack = global_stack; //updates saved stack to current global stack pointer

#endif