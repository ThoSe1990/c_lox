#ifndef clox_compiler_h
#define clox_compiler_h

#include "object.h"
#include "vm.h"

obj_function* compile(const char* source);

#endif 