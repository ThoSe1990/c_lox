#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "debug.h"
#include "object.h"
#include "memory.h"
#include "vm.h"
#include "compiler.h"

vm g_vm;

static void reset_stack()
{
  g_vm.stack_top = g_vm.stack;
}

static void runtime_error(const char* format, ...)
{
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  fputs("\n", stderr);

  size_t instruction = g_vm.ip - g_vm.chunk->code - 1;
  int line = g_vm.chunk->lines[instruction];
  fprintf(stderr, "[line %d] in script\n", line);
  reset_stack();
}

void init_vm()
{
  reset_stack();
  g_vm.objects = NULL;
  init_table(&g_vm.globals);
  init_table(&g_vm.strings);
}
void free_vm()
{
  free_table(&g_vm.globals);
  free_table(&g_vm.strings);
  free_objects();
}

void push(value v)
{
  *g_vm.stack_top = v;
  g_vm.stack_top++;
}
value pop()
{
  g_vm.stack_top--;
  return *g_vm.stack_top;
}

static value peek(int distance)
{
  return g_vm.stack_top[-1 - distance];
}

static bool is_falsey(value v)
{
  return IS_NIL(v) || (IS_BOOL(v) && !AS_BOOL(v)); 
}

static void concatenate()
{
  obj_string* b = AS_STRING(pop());
  obj_string* a = AS_STRING(pop());
  int length = a->length + b->length;
  char* chars = ALLOCATE(char, length+1);
  memcpy(chars, a->chars, a->length);
  memcpy(chars + a->length, b->chars, b->length);
  chars[length] = '\0';

  obj_string* result = take_string(chars, length);
  push(OBJ_VAL(result));
}

static interpret_result run() 
{
#define READ_BYTE() (*g_vm.ip++)
#define READ_CONSTANT() (g_vm.chunk->constants.values[READ_BYTE()])
#define READ_STRING() AS_STRING(READ_CONSTANT())
#define BINARY_OP(value_t, op) \
  do { \
    if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) { \
      runtime_error("Operand must be numbers."); \
      return INTERPRET_RUNTIME_ERROR; \
    } \
    double b = AS_NUMBER(pop()); \
    double a = AS_NUMBER(pop()); \
    push(value_t(a op b)); \
  } while (false)

  for(;;)
  {
#ifdef DEBUG_TRACE_EXTENSION
  printf("        ");
  for (value* slot = g_vm.stack; slot < g_vm.stack_top; slot++)
  {
    printf("[ ");
    print_value(*slot);
    printf(" ]");
  }
  printf("\n");
  disassemble_instruction(g_vm.chunk, (int)(g_vm.ip - g_vm.chunk->code));
#endif 

    uint8_t instruction; 
    switch (instruction = READ_BYTE()) 
    {
      case OP_CONSTANT:
      {
        value constant = READ_CONSTANT();
        push(constant);
      }
      break; case OP_NIL: push(NIL_VAL);
      break; case OP_TRUE: push(BOOL_VAL(true));
      break; case OP_FALSE: push(BOOL_VAL(false));
      break; case OP_POP: pop(); 
      break; case OP_SET_LOCAL: 
      {
        uint8_t slot = READ_BYTE();
        g_vm.stack[slot] = peek(0);
      }
      break; case OP_GET_LOCAL: 
      {
        uint8_t slot = READ_BYTE();
        push(g_vm.stack[slot]);
      }
      break; case OP_GET_GLOBAL:
      {
        obj_string* name = READ_STRING();
        value v; 
        if (!table_get(&g_vm.globals, name, &v))
        {
          runtime_error("Undefined variable '%s'.", name->chars);
          return INTERPRET_RUNTIME_ERROR;
        }
        else 
        {
          push(v);
        }
      }
      break; case OP_DEFINE_GLOBAL:
      {
        obj_string* name = READ_STRING();
        table_set(&g_vm.globals, name, peek(0));
        pop();
      }
      break; case OP_SET_GLOBAL:
      {
        obj_string* name = READ_STRING();
        if (table_set(&g_vm.globals, name, peek(0)))
        {
          table_delete(&g_vm.globals, name);
          runtime_error("Undefined variable '%s'.", name->chars);
          return INTERPRET_RUNTIME_ERROR;
        }
      }
      break; case OP_EQUAL:
        value a = pop();
        value b = pop();
        push(BOOL_VAL(values_equal(a,b)));
      break; case OP_GREATER:          BINARY_OP(BOOL_VAL, >);
      break; case OP_LESS:             BINARY_OP(BOOL_VAL, <);
      break; case OP_ADD:         
      {
        if (IS_STRING(peek(0)) && IS_STRING(peek(1)))
        {
          concatenate();
        }
        else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1)))
        {
          double a = AS_NUMBER(pop());
          double b = AS_NUMBER(pop());
          push(NUMBER_VAL(a+b));
        }
        else 
        {
          runtime_error("Operands must be two numbers or two strings");
          return INTERPRET_RUNTIME_ERROR;
        }
      }
      break; case OP_SUBTRACT:    BINARY_OP(NUMBER_VAL, -);
      break; case OP_MULTIPLY:    BINARY_OP(NUMBER_VAL, *);
      break; case OP_DIVIDE:      BINARY_OP(NUMBER_VAL, /);
      break; case OP_NOT: push(BOOL_VAL(is_falsey(pop())));
      break; case OP_NEGATE: 
        if (!IS_NUMBER(peek(0))) 
        {
          runtime_error("Operand must be a number.");
          return INTERPRET_RUNTIME_ERROR;
        }
        push(NUMBER_VAL(-AS_NUMBER(pop())));  
      break; case OP_PRINT:
      {
        print_value(pop());
        printf("\n");
      }
      break; case OP_RETURN: 
      {
        // exit the interpreter
        return INTERPRET_OK;
      }

    }
  }

#undef READ_BYTE
#undef READ_CONSTANT
#undef READ_STRING
#undef BINARY_OP
}

interpret_result interpret(const char* source)
{
  chunk c;
  init_chunk(&c);

  if(!compile(source, &c))
  {
    free_chunk(&c);
    return INTERPRET_COMPILE_ERROR;
  }

  g_vm.chunk = &c;
  g_vm.ip = g_vm.chunk->code;

  interpret_result result = run();

  free_chunk(&c);
  return result; 
}

