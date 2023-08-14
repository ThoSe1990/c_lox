#include <stdio.h>

#include "debug.h"

void disassemble_chunk(chunk* c, const char* name)
{
  printf("== %s ==", name);
  for (int offset = 0; offset < c->count;)
  {
    offset = disassemble_instruction(c, offset);
  }
}
int disassemble_instruction(chunk* c, int offset)
{
  printf("%04d ", offset);
  uint8_t instruction = c->code[offset];
  switch (instruction)
  {
  case OP_RETURN:
    return simple_instruction("OP_RETURN", offset);
  default:
    printf("Unknown opcode %d\n", instruction);
    return offset+1;
  }
}

static int simple_instruction(const char* name, int offset)
{
  printf("%s\n", name);
  return offset+1;
}