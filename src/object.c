#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "value.h"
#include "vm.h"


#define ALLOCATE_OBJ(type, object_type) \
  (type*)allocate_object(sizeof(type), object_type)

static obj* allocate_object(size_t size, obj_type type) 
{
  obj* object = (obj*)reallocate(NULL, 0, size);
  object->type = type;
  object->next = g_vm.objects;
  g_vm.objects = object;
  return object;
}

static obj_string* allocate_string(char* chars, int length)
{
  obj_string* str = ALLOCATE_OBJ(obj_string, OBJ_STRING);
  str->length = length;
  str->chars = chars;
  return str;
}
obj_string* take_string(char* chars, int length)
{
  return allocate_string(chars, length);
}
obj_string* copy_string(const char* chars, int length)
{
  char* heap_chars = ALLOCATE(char, length+1);
  memcpy(heap_chars, chars, length);
  heap_chars[length] = '\0';
  return allocate_string(heap_chars, length);
}

void print_object(value v)
{
  switch(OBJ_TYPE(v))
  {
    case OBJ_STRING: printf("%s", AS_CSTRING(v));
    break;
  }
}