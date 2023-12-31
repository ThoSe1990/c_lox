#ifndef clox_table_h
#define clox_table_h

#include "common.h"
#include "value.h"

typedef struct {
  obj_string* key;
  value value;
} entry;

typedef struct {
  int count;
  int capacity;
  entry* entries; 
} table;

void init_table(table* t);
void free_table(table* t);
bool table_set(table* t, obj_string* key, value v);
bool table_get(table* t, obj_string* key, value* v);
bool table_delete(table* t, obj_string* key);
void table_add_all(table* from, table* to);
obj_string* table_find_string(table* t, const char* chars, int length, uint32_t hash);

#endif 
