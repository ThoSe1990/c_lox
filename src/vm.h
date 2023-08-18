#ifndef clox_vm_h
#define clox_vm_h

#include "chunk.h"

#define STACK_MAX 256

typedef struct {
  chunk* chunk;
  uint8_t* ip;
  value stack[STACK_MAX];
  value* stack_top;
  obj* objects;
} vm;

typedef enum {
  INTERPRET_OK, 
  INTERPRET_COMPILE_ERROR,
  INTERPRET_RUNTIME_ERROR
} interpret_result;

extern vm g_vm;

void init_vm();
void free_vm();
interpret_result interpret(const char* source);
void push(value value);
value pop();

#endif 