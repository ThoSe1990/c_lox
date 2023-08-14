#include "common.h"
#include "chunk.h"

#include "debug.h"

int main(int argc, const char* argv[])
{
  printf("start program!\n");

  chunk c; 
  init_chunk(&c);
  int constant = add_constant(&c, 1.2);
  write_chunk(&c, OP_CONSTANT, 123);
  write_chunk(&c, constant, 123);

  write_chunk(&c, OP_RETURN, 123);

  disassemble_chunk(&c, "test chunk");

  free_chunk(&c);

  printf("end program!\n");

  return 0;
}