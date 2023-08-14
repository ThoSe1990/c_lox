#include "chunk.h"
#include "memory.h"

void init_chunk(chunk* c)
{
  c->count = 0;
  c->capacity = 0;
  c->code = NULL;
}

void free_chunk(chunk* c)
{
  FREE_ARRAY(uint8_t, c->code, c->capacity);
  init_chunk(c);
}

void write_chunk(chunk* c, uint8_t byte)
{
  if (c->capacity < c->count + 1)
  {
    int old_cap = c->capacity;
    c->capacity = GROW_CAPACITY(old_cap);
    c->code = GROW_ARRAY(uint8_t, c->code, old_cap, c->capacity);
  }

  c->code[c->count] = byte;
  ++c->count;
}

