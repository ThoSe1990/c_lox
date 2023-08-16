#ifndef clox_value_h
#define clox_value_h

#include "common.h"

typedef enum {
  VAL_BOOL,
  VAL_NIL,
  VAL_NUMBER
} value_type;

typedef struct {
  value_type type;
  union {
    bool boolean;
    double number;
  } as;
} value;

#define BOOL_VAL(v)   ((value){VAL_BOOL, {.boolean = v}})
#define NIL_VAL       ((value){VAL_NIL, {.number = 0}})
#define NUMBER_VAL(v) ((value){VAL_NUMBER, {.number = v}})

#define AS_BOOL(v)    ((v).as.boolean)
#define AS_NUMBER(v)  ((v).as.number)

#define IS_BOOL(v)    ((v).type == VAL_BOOL)
#define IS_NIL(v)     ((v).type == VAL_NIL)
#define IS_NUMBER(v)  ((v).type == VAL_NUMBER)

typedef struct {
  int capacity;
  int count;
  value* values;
} value_array;

bool values_equal(value a, value b);
void init_value_array(value_array* arr);
void write_value_array(value_array* arr, value v);
void free_value_array(value_array* arr);
void print_value(value v);

#endif 