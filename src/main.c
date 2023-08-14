#include "common.h"
#include "chunk.h"

#include "debug.h"

int main(int argc, const char* argv[])
{
  printf("start program!\n");

  chunk c; 
  init_chunk(&c);
  write_chunk(&c, OP_RETURN);

  disassemble_chunk(&c, "test chunk");

  free_chunk(&c);

  printf("end program!\n");

  return 0;
}