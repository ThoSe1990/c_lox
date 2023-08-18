#ifndef clox_object_h
#define clox_object_h

#include "common.h"
#include "value.h"

#define OBJ_TYPE(v)     (AS_OBJ(v)->type)

#define IS_STRING(v)    is_obj_type(v, OBJ_STRING)

#define AS_STRING(v)    ((obj_string*)AS_OBJ(v))
#define AS_CSTRING(v)    (((obj_string*)AS_OBJ(v))->chars)

typedef enum {
  OBJ_STRING
} obj_type;

struct obj {
  obj_type type;
  struct obj* next;
};

struct obj_string {
  obj object;
  int length;
  char* chars;
};

obj_string* take_string(char* chars, int length);
obj_string* copy_string(const char* chars, int length);
void print_object(value v);

static inline bool is_obj_type(value v, obj_type type) 
{
  return IS_OBJ(v) && AS_OBJ(v)->type == type;
}

#endif 