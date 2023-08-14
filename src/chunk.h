#ifndef chunk_common_h
#define chunk_common_h

#include "common.h"
#include "value.h"

typedef enum {
  OP_CONSTANT,
  OP_NEGATE, 
  OP_ADD,
  OP_SUBTRACT,
  OP_MULTIPLY,
  OP_DIVIDE,
  OP_RETURN
} op_code;

typedef struct
{
  int count;
  int capacity;
  uint8_t* code;
  int* lines;
  value_array constants;
} chunk;

void init_chunk(chunk* c);
void free_chunk(chunk* c);
void write_chunk(chunk* c, uint8_t byte, int line);
int add_constant(chunk* c, value_t v);

#endif