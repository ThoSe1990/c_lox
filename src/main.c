#include "common.h"
#include "chunk.h"

#include "debug.h"
#include "vm.h"

int main(int argc, const char* argv[])
{
  printf("start program!\n");

  init_vm();

  chunk c; 
  init_chunk(&c);
  
  int constant = add_constant(&c, 5.0);
  write_chunk(&c, OP_CONSTANT, 123);
  write_chunk(&c, constant, 123);
  
  constant = add_constant(&c, 3.0);
  write_chunk(&c, OP_CONSTANT, 123);
  write_chunk(&c, constant, 123);
  
  write_chunk(&c, OP_ADD, 123);

  constant = add_constant(&c, 4.0);
  write_chunk(&c, OP_CONSTANT, 123);
  write_chunk(&c, constant, 123);
  
  write_chunk(&c, OP_DIVIDE, 123);

  write_chunk(&c, OP_NEGATE, 123);


  write_chunk(&c, OP_RETURN, 123);

  interpret(&c);
  free_vm();
  free_chunk(&c);

  printf("end program!\n");

  return 0;
}