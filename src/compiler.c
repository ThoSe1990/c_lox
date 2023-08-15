#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "compiler.h"
#include "scanner.h"

#ifdef DEBUG_PRINT_CODE
  #include "debug.h"
#endif

typedef struct {
  token current;
  token previous; 
  bool had_error;
  bool panic_mode;
} parser_t;

typedef enum {
  PREC_NONE,
  PREC_ASSIGNMENT, 
  PREC_OR,
  PREC_AND,
  PREC_EQUALITY,
  PREC_COMPARISON,
  PREC_TERM,
  PREC_FACTOR,
  PREC_UNARY,
  PREC_CALL,
  PREC_PRIMARY
} precedence_type;

typedef void (*parse_fn)();

typedef struct {
  parse_fn prefix;
  parse_fn infix;
  precedence_type precedence;
} parse_rule;

parser_t parser; 
chunk* compiling_chunk;

static chunk* current_chunk()
{
  return compiling_chunk;
}

static void error_at(token* t, const char* msg)
{
  if (parser.panic_mode) 
  {
    return;
  }
  parser.panic_mode = true;
  
  fprintf(stderr, "[line %d] Error", t->line);
  if (t->type == TOKEN_EOF)
  {
    fprintf(stderr, " at end");
  }
  else if (t->type == TOKEN_ERROR)
  {
    // nothing
  }
  else 
  {
    fprintf(stderr, " at '%.*s'", t->length, t->start);
  }

  fprintf(stderr, ": %s\n", msg);
  parser.had_error = true;
}

static void error(const char* msg) 
{
  error_at(&parser.previous, msg);
}

static void error_at_current(const char* msg) 
{
  error_at(&parser.current, msg);
}

static void advance()
{
  parser.previous = parser.current;

  for (;;) 
  {
    parser.current = scan_token();
    if (parser.current.type != TOKEN_ERROR) 
    {
      break;
    }
    error_at_current(parser.current.start);
  }
}

static void consume(token_type type, const char* msg)
{
  if (parser.current.type == type) 
  {
    advance();
    return;
  }

  error_at_current(msg);
}

static void emit_byte(uint8_t byte)
{
  write_chunk(current_chunk(), byte, parser.previous.line);
}

static void emit_bytes(uint8_t byte1, uint8_t byte2)
{
  emit_byte(byte1);
  emit_byte(byte2);
}

static void emit_return()
{
  emit_byte(OP_RETURN);
}

static uint8_t make_constant(value_t value)
{
  int constant = add_constant(current_chunk(), value);
  if (constant > UINT8_MAX)
  {
    error("Too many constants in one chunk");
    return 0;
  }
  return (uint8_t)constant;
}

static void emit_constant(value_t value)
{
  emit_bytes(OP_CONSTANT, make_constant(value));
}



static void end_compiler()
{
#ifdef DEBUG_PRINT_CODE
  if (!parser.had_error)
  {
    disassemble_chunk(current_chunk(), "code");
  }
#endif
  emit_return();
}

static void expression();
static parse_rule* get_rule(token_type type);
static void parse_precedence(precedence_type precedence);


static void grouping()
{
  expression();
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void binary()
{
  token_type operator_type = parser.previous.type;
  parse_rule* rule = get_rule(operator_type);
  parse_precedence((precedence_type)(rule->precedence + 1));
  switch (operator_type)
  {
    case TOKEN_PLUS: emit_byte(OP_ADD); break;
    case TOKEN_MINUS: emit_byte(OP_SUBTRACT); break;
    case TOKEN_STAR: emit_byte(OP_MULTIPLY); break;
    case TOKEN_SLASH: emit_byte(OP_DIVIDE); break;
    default: return; // unreachable
  }
}

static void number()
{
  double value = strtod(parser.previous.start, NULL);
  emit_constant(value);
}

static void unary() 
{
  token_type operator_type = parser.previous.type;

  parse_precedence(PREC_UNARY);

  switch (operator_type)
  {
    case TOKEN_MINUS: emit_byte(OP_NEGATE); break;
    default: return;
  }
}

parse_rule rules[] = {
  [TOKEN_LEFT_PAREN]    = {grouping,  NULL,     PREC_NONE},
  [TOKEN_RIGHT_PAREN]   = {NULL,      NULL,     PREC_NONE},
  [TOKEN_LEFT_BRACE]    = {NULL,      NULL,     PREC_NONE}, 
  [TOKEN_RIGHT_BRACE]   = {NULL,      NULL,     PREC_NONE},
  [TOKEN_COMMA]         = {NULL,      NULL,     PREC_NONE},
  [TOKEN_DOT]           = {NULL,      NULL,     PREC_NONE},
  [TOKEN_MINUS]         = {unary,     binary,   PREC_TERM},
  [TOKEN_PLUS]          = {NULL,      binary,   PREC_TERM},
  [TOKEN_SEMICOLON]     = {NULL,      NULL,     PREC_NONE},
  [TOKEN_SLASH]         = {NULL,      binary,   PREC_FACTOR},
  [TOKEN_STAR]          = {NULL,      binary,   PREC_FACTOR},
  [TOKEN_BANG]          = {NULL,      NULL,     PREC_NONE},
  [TOKEN_BANG_EQUAL]    = {NULL,      NULL,     PREC_NONE},
  [TOKEN_EQUAL]         = {NULL,      NULL,     PREC_NONE},
  [TOKEN_EQUAL_EQUAL]   = {NULL,      NULL,     PREC_NONE},
  [TOKEN_GREATER]       = {NULL,      NULL,     PREC_NONE},
  [TOKEN_GREATER_EQUAL] = {NULL,      NULL,     PREC_NONE},
  [TOKEN_LESS]          = {NULL,      NULL,     PREC_NONE},
  [TOKEN_LESS_EQUAL]    = {NULL,      NULL,     PREC_NONE},
  [TOKEN_IDENTIFIER]    = {NULL,      NULL,     PREC_NONE},
  [TOKEN_STRING]        = {NULL,      NULL,     PREC_NONE},
  [TOKEN_NUMBER]        = {number,    NULL,     PREC_NONE},
  [TOKEN_AND]           = {NULL,      NULL,     PREC_NONE},
  [TOKEN_CLASS]         = {NULL,      NULL,     PREC_NONE},
  [TOKEN_ELSE]          = {NULL,      NULL,     PREC_NONE},
  [TOKEN_FALSE]         = {NULL,      NULL,     PREC_NONE},
  [TOKEN_FOR]           = {NULL,      NULL,     PREC_NONE},
  [TOKEN_FUN]           = {NULL,      NULL,     PREC_NONE},
  [TOKEN_IF]            = {NULL,      NULL,     PREC_NONE},
  [TOKEN_NIL]           = {NULL,      NULL,     PREC_NONE},
  [TOKEN_OR]            = {NULL,      NULL,     PREC_NONE},
  [TOKEN_PRINT]         = {NULL,      NULL,     PREC_NONE},
  [TOKEN_RETURN]        = {NULL,      NULL,     PREC_NONE},
  [TOKEN_SUPER]         = {NULL,      NULL,     PREC_NONE},
  [TOKEN_THIS]          = {NULL,      NULL,     PREC_NONE},
  [TOKEN_TRUE]          = {NULL,      NULL,     PREC_NONE},
  [TOKEN_VAR]           = {NULL,      NULL,     PREC_NONE},
  [TOKEN_WHILE]         = {NULL,      NULL,     PREC_NONE},
  [TOKEN_ERROR]         = {NULL,      NULL,     PREC_NONE},
  [TOKEN_EOF]           = {NULL,      NULL,     PREC_NONE},
};




static void parse_precedence(precedence_type precendence)
{
  advance();
  parse_fn prefix_rule = get_rule(parser.previous.type)->prefix;
  if(prefix_rule == NULL)
  {
    error("Expect expression.");
    return;
  }
  prefix_rule();

  while(precendence <= get_rule(parser.current.type)->precedence) 
  {
    advance();
    parse_fn infix_rule = get_rule(parser.previous.type)->infix;
    infix_rule();
  }
}

static parse_rule* get_rule(token_type type)
{
  return &rules[type];
}

static void expression() 
{
  parse_precedence(PREC_ASSIGNMENT);
}


bool compile(const char* source, chunk* c)
{
  init_scanner(source);
  compiling_chunk = c;

  parser.had_error = false;
  parser.panic_mode = false;

  advance();
  expression();
  consume(TOKEN_EOF, "Expect end of expression");
  end_compiler();
  return !parser.had_error;
}
