#ifndef clox_value_h
#define clox_value_h


#include "common.h"

typedef double value_t;

typedef struct {
  int capacity;
  int count;
  value_t* values;
} value_array;

void init_value_array(value_array* arr);
void write_value_array(value_array* arr, value_t value);
void free_value_array(value_array* arr);
void print_value(value_t value);

#endif 