#ifndef chunk_common_h
#define chunk_common_h

#include "common.h"

typedef enum {
  OP_RETURN
} op_code;

typedef struct
{
  int count;
  int capacity;
  uint8_t* code;
} chunk;

void init_chunk(chunk* c);
void free_chunk(chunk* c);
void write_chunk(chunk* c, uint8_t byte);

#endif