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