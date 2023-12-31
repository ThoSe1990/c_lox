#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

typedef void (*parse_fn)(bool can_assign);

typedef struct {
  parse_fn prefix;
  parse_fn infix;
  precedence_type precedence;
} parse_rule;

typedef struct {
  token name; 
  int depth;
} local;

typedef enum {
  TYPE_FUNCTION,
  TYPE_SCRIPT
} function_type;

typedef struct compiler {
  struct compiler* enclosing;
  obj_function* function;
  function_type type;
  local locals[UINT8_COUNT];
  int local_count;
  int scope_depth;
} compiler;


parser_t parser; 
compiler* current = NULL;

static chunk* current_chunk() 
{
  return &current->function->chunk;
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

static bool check(token_type type)
{
  return parser.current.type == type;
}

static bool match(token_type type)
{
  if (!check(type)) 
  { 
    return false; 
  }
  else 
  {
    advance();
    return true;
  }
}

static void emit_byte(uint8_t byte)
{
  write_chunk(current_chunk(), byte, parser.previous.line);
}

static void emit_loop(uint8_t loop_start)
{
  emit_byte(OP_LOOP);

  int offset = current_chunk()->count - loop_start + 2;
  if (offset > UINT16_MAX)
  {
    error("Loop body too large.");
  }
  emit_byte((offset >> 8) & 0xff);
  emit_byte(offset & 0xff);
}

static void emit_bytes(uint8_t byte1, uint8_t byte2)
{
  emit_byte(byte1);
  emit_byte(byte2);
}

static int emit_jump(uint8_t instruction)
{
  emit_byte(instruction);
  emit_byte(0xff);
  emit_byte(0xff);
  return current_chunk()->count - 2;
}

static void emit_return()
{
  emit_byte(OP_NIL);
  emit_byte(OP_RETURN);
}

static uint8_t make_constant(value v)
{
  int constant = add_constant(current_chunk(), v);
  if (constant > UINT8_MAX)
  {
    error("Too many constants in one chunk");
    return 0;
  }
  return (uint8_t)constant;
}

static void emit_constant(value v)
{
  emit_bytes(OP_CONSTANT, make_constant(v));
}

static void patch_jump(int offset)
{
  int jump = current_chunk()->count - offset - 2;
  if (jump > UINT16_MAX)
  {
    error("Too much code to jump over.");
  }
  current_chunk()->code[offset] = (jump >> 8) & 0xff;
  current_chunk()->code[offset+1] = jump & 0xff;
}

static void init_compiler(compiler* c, function_type type)
{
  c->enclosing = current;
  c->function = NULL;
  c->type = type;
  c->local_count = 0;
  c->scope_depth = 0; 
  c->function = new_function();
  current = c;

  if (type != TYPE_SCRIPT)
  {
    current->function->name = copy_string(parser.previous.start, parser.previous.length);
  }

  local* l = &current->locals[current->local_count++];
  l->depth = 0;
  l->name.start = "";
  l->name.length = 0;
}

static obj_function* end_compiler()
{
  emit_return();
  obj_function* func = current->function;

#ifdef DEBUG_PRINT_CODE
  if (!parser.had_error)
  {
    disassemble_chunk(current_chunk(), 
      func->name != NULL ? func->name->chars : "<script>");
  }
#endif
  
  current = current->enclosing;
  return func;
}

static void begin_scope()
{
  current->scope_depth++;
}
static void end_scope()
{
  current->scope_depth--;
  while (current->local_count > 0 
    && current->locals[current->local_count-1].depth > current->scope_depth)
  {
    emit_byte(OP_POP);
    current->local_count--;
  }
}

static void expression();
static void statement();
static void declaration();
static parse_rule* get_rule(token_type type);
static void parse_precedence(precedence_type precedence);


static void grouping(bool can_assign)
{
  expression();
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void binary(bool can_assign)
{
  token_type operator_type = parser.previous.type;
  parse_rule* rule = get_rule(operator_type);
  parse_precedence((precedence_type)(rule->precedence + 1));
  switch (operator_type)
  {
    case TOKEN_BANG_EQUAL:            emit_bytes(OP_EQUAL, OP_NOT);
    break; case TOKEN_EQUAL_EQUAL:    emit_byte(OP_EQUAL);
    break; case TOKEN_GREATER:        emit_byte(OP_GREATER);
    break; case TOKEN_GREATER_EQUAL:  emit_bytes(OP_LESS, OP_NOT);
    break; case TOKEN_LESS:           emit_byte(OP_LESS);
    break; case TOKEN_LESS_EQUAL:     emit_bytes(OP_GREATER, OP_NOT);
    break; case TOKEN_PLUS:           emit_byte(OP_ADD); 
    break; case TOKEN_MINUS:          emit_byte(OP_SUBTRACT); 
    break; case TOKEN_STAR:           emit_byte(OP_MULTIPLY); 
    break; case TOKEN_SLASH:          emit_byte(OP_DIVIDE); 
    default: return; // unreachable
  }
}

static void literal(bool can_assign)
{
  switch (parser.previous.type)
  {
  case TOKEN_FALSE: emit_byte(OP_FALSE); 
  break; case TOKEN_NIL: emit_byte(OP_NIL);
  break; case TOKEN_TRUE: emit_byte(OP_TRUE);
  default: return; // unreachable
  }
}

static void number(bool can_assign)
{
  double number = strtod(parser.previous.start, NULL);
  emit_constant(NUMBER_VAL(number));
}

static void string(bool can_assign) 
{
  emit_constant(OBJ_VAL(copy_string(parser.previous.start+1 , parser.previous.length-2)));
}

static uint8_t identifier_constant(token* name)
{
  return make_constant(OBJ_VAL(copy_string(name->start, name->length)));
}

static bool identifiers_equal(token* a, token* b)
{
  if (a->length != b->length)
  {
    return false;
  }
  return memcmp(a->start, b->start, a->length) == 0;
}

static int resolve_local(compiler* c, token* name)
{
  for (int i = c->local_count-1 ; i >= 0 ; i--)
  {
    local* l = &c->locals[i];
    if (identifiers_equal(name, &l->name)) 
    {
      if (l->depth == -1) 
      {
        error("Can't read local variable in its own initializer.");
      }
      return i;
    }
  }
  return -1;
}

static void add_local(token name)
{
  if (current->local_count == UINT8_COUNT)
  {
    error("Too many local variables in function.");
  }
  else 
  {
    local* l = &current->locals[current->local_count++];
    l->name = name;
    l->depth = -1;
  }
  
}

static void declare_variable()
{
  if (current->scope_depth == 0) 
  {
    return;
  }
  else 
  {
    token* name = &parser.previous;

    for (int i = current->local_count - 1 ; i >= 0 ; i--)
    {
      local* l = &current->locals[i];
      if (l->depth != -1 && l->depth < current->scope_depth)
      {
        break;
      }
      if (identifiers_equal(name, &l->name)) 
      {
        error("Already a variable with this name in this scope");
      }
    }

    add_local(*name);
  }
}

static void named_variable(token name, bool can_assign)
{
  uint8_t get_op, set_op;
  int arg = resolve_local(current, &name);
  if (arg != -1) 
  {
    get_op = OP_GET_LOCAL;
    set_op = OP_SET_LOCAL;
  }
  else 
  {
    arg = identifier_constant(&name);
    get_op = OP_GET_GLOBAL;
    set_op = OP_SET_GLOBAL;
  }
  
  if (can_assign && match(TOKEN_EQUAL))
  {
    expression();
    emit_bytes(set_op, (uint8_t)arg);
  }
  else 
  {
    emit_bytes(get_op, (uint8_t)arg);
  }
}

static void variable(bool can_assign)
{
  named_variable(parser.previous, can_assign);
}

static void unary(bool can_assign) 
{
  token_type operator_type = parser.previous.type;

  parse_precedence(PREC_UNARY);

  switch (operator_type)
  {
    case TOKEN_BANG: emit_byte(OP_NOT); 
    break; case TOKEN_MINUS: emit_byte(OP_NEGATE); 
    break; default: return;
  }
}

static void and_(bool can_assign)
{
  int end_jump = emit_jump(OP_JUMP_IF_FALSE);
  emit_byte(OP_POP);
  parse_precedence(PREC_AND);
  patch_jump(end_jump);
}

static void or_(bool can_assign)
{
  int else_jump = emit_jump(OP_JUMP_IF_FALSE);
  int end_jump = emit_jump(OP_JUMP);

  patch_jump(else_jump);
  emit_byte(OP_POP);
  
  parse_precedence(PREC_OR);
  patch_jump(end_jump);
}

static uint8_t argument_list()
{
  uint8_t arg_count = 0;
  if (!check(TOKEN_RIGHT_PAREN))
  {
    do 
    {
      expression();
      if (arg_count == 255) 
      {
        error("Can't have more than 255 arguments.");
      }
      arg_count++;
    } while (match(TOKEN_COMMA));
  }
  consume(TOKEN_RIGHT_PAREN,"Expect ')' after arguments.");
  return arg_count;
}

static void call(bool can_assign)
{
  uint8_t arg_count = argument_list();
  emit_bytes(OP_CALL, arg_count);
}

parse_rule rules[] = {
  [TOKEN_LEFT_PAREN]    = {grouping,  call,     PREC_CALL},
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
  [TOKEN_BANG]          = {unary,     NULL,     PREC_NONE},
  [TOKEN_BANG_EQUAL]    = {NULL,      binary,   PREC_EQUALITY},
  [TOKEN_EQUAL]         = {NULL,      NULL,     PREC_NONE},
  [TOKEN_EQUAL_EQUAL]   = {NULL,      binary,   PREC_EQUALITY},
  [TOKEN_GREATER]       = {NULL,      binary,   PREC_COMPARISON},
  [TOKEN_GREATER_EQUAL] = {NULL,      binary,   PREC_COMPARISON},
  [TOKEN_LESS]          = {NULL,      binary,   PREC_COMPARISON},
  [TOKEN_LESS_EQUAL]    = {NULL,      binary,   PREC_COMPARISON},
  [TOKEN_IDENTIFIER]    = {variable,      NULL,     PREC_NONE},
  [TOKEN_STRING]        = {string,      NULL,     PREC_NONE},
  [TOKEN_NUMBER]        = {number,    NULL,     PREC_NONE},
  [TOKEN_AND]           = {NULL,      and_,     PREC_AND},
  [TOKEN_CLASS]         = {NULL,      NULL,     PREC_NONE},
  [TOKEN_ELSE]          = {NULL,      NULL,     PREC_NONE},
  [TOKEN_FALSE]         = {literal,   NULL,     PREC_NONE},
  [TOKEN_FOR]           = {NULL,      NULL,     PREC_NONE},
  [TOKEN_FUN]           = {NULL,      NULL,     PREC_NONE},
  [TOKEN_IF]            = {NULL,      NULL,     PREC_NONE},
  [TOKEN_NIL]           = {literal,   NULL,     PREC_NONE},
  [TOKEN_OR]            = {NULL,      or_,     PREC_OR},
  [TOKEN_PRINT]         = {NULL,      NULL,     PREC_NONE},
  [TOKEN_RETURN]        = {NULL,      NULL,     PREC_NONE},
  [TOKEN_SUPER]         = {NULL,      NULL,     PREC_NONE},
  [TOKEN_THIS]          = {NULL,      NULL,     PREC_NONE},
  [TOKEN_TRUE]          = {literal,   NULL,     PREC_NONE},
  [TOKEN_VAR]           = {NULL,      NULL,     PREC_NONE},
  [TOKEN_WHILE]         = {NULL,      NULL,     PREC_NONE},
  [TOKEN_ERROR]         = {NULL,      NULL,     PREC_NONE},
  [TOKEN_EOF]           = {NULL,      NULL,     PREC_NONE},
};




static void parse_precedence(precedence_type precedence)
{
  advance();
  parse_fn prefix_rule = get_rule(parser.previous.type)->prefix;
  if(prefix_rule == NULL)
  {
    error("Expect expression.");
    return;
  }
  
  bool can_assign = precedence <= PREC_ASSIGNMENT;
  prefix_rule(can_assign);

  while(precedence <= get_rule(parser.current.type)->precedence) 
  {
    advance();
    parse_fn infix_rule = get_rule(parser.previous.type)->infix;
    infix_rule(can_assign);
  }
  if (can_assign && match(TOKEN_EQUAL))
  {
    error("Invalid assignment target.");
  }
}


static uint8_t parse_variable(const char* errormsg)
{
  consume(TOKEN_IDENTIFIER, errormsg);
  declare_variable();
  if (current->scope_depth > 0) { return 0; }
  return identifier_constant(&parser.previous);
}

static void mark_initialized()
{
  if (current->scope_depth == 0)
  {
    return ;
  }
  else 
  {
    current->locals[current->local_count-1].depth = current->scope_depth;
  }
}

static void define_variable(uint8_t global)
{
  if (current->scope_depth > 0) 
  {
    mark_initialized();
    return ;
  }
  emit_bytes(OP_DEFINE_GLOBAL, global);
}


static parse_rule* get_rule(token_type type)
{
  return &rules[type];
}

static void expression() 
{
  parse_precedence(PREC_ASSIGNMENT);
}

static void block()
{
  while(!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF))
  {
    declaration();
  }
  consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

static void function(function_type type)
{
  compiler c;
  init_compiler(&c, type);
  begin_scope();
  
  consume(TOKEN_LEFT_PAREN, "Expect '(' after function name.");
  if(!check(TOKEN_RIGHT_PAREN))
  {
    do 
    {
      current->function->arity++;
      if(current->function->arity > 255)
      {
        error_at_current("Can't have more than 255 parameters");
      }
      uint8_t constant = parse_variable("Expect parameter name");
      define_variable(constant);
    }
    while (match(TOKEN_COMMA));
  }
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");
  consume(TOKEN_LEFT_BRACE, "Expect '{' before function body.");

  block();

  obj_function* func = end_compiler();
  emit_bytes(OP_CONSTANT, make_constant(OBJ_VAL(func)));
}

static void fun_declaration()
{
  uint8_t global = parse_variable("Expect function name");
  mark_initialized();
  function(TYPE_FUNCTION);
  define_variable(global);
}

static void var_declaration()
{
  uint8_t global = parse_variable("Expect variable name.");
  if (match(TOKEN_EQUAL))
  {
    expression();
  }
  else 
  {
    emit_byte(OP_NIL);
  }
  consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");
  define_variable(global);
}

static void expression_statement()
{
  expression();
  consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
  emit_byte(OP_POP);
}

static void for_statement() 
{
  begin_scope();

  consume(TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");
  
  if (match(TOKEN_SEMICOLON))
  {
    // no initializer... 
  }
  else if (match(TOKEN_VAR)) 
  {
    var_declaration();
  }
  else 
  {
    expression_statement();
  }

  int loop_start = current_chunk()->count;
  int exit_jump = -1;

  if (!match(TOKEN_SEMICOLON))
  {
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after loop condition.");
    // jump out if condition is false .. 
    exit_jump = emit_jump(OP_JUMP_IF_FALSE);
    emit_byte(OP_POP);
  }

  if (!match(TOKEN_RIGHT_PAREN))
  {
    int body_jump = emit_jump(OP_JUMP);
    int increment_start = current_chunk()->count;
    expression();
    emit_byte(OP_POP);
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");

    emit_loop(loop_start);
    loop_start = increment_start;
    patch_jump(body_jump);
  }

  statement();
  emit_loop(loop_start);

  if (exit_jump != -1) 
  {
    patch_jump(exit_jump);
    emit_byte(OP_POP);
  }

  end_scope();
}

static void if_statement()
{
  consume(TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
  expression();
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

  int then_jump = emit_jump(OP_JUMP_IF_FALSE);
  emit_byte(OP_POP);
  statement();

  int else_jump = emit_jump(OP_JUMP);

  patch_jump(then_jump);
  emit_byte(OP_POP);
  if (match(TOKEN_ELSE))
  {
    statement();
  }
  patch_jump(else_jump);

}

static void print_statement()
{
  expression();
  consume(TOKEN_SEMICOLON, "Expect ';' after value.");
  emit_byte(OP_PRINT);
}

static void return_statement()
{
  if (current->type == TYPE_SCRIPT)
  {
    error("Can't return from top-level code.");
  }

  if (match(TOKEN_SEMICOLON))
  {
    emit_return();
  }
  else 
  {
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after return value.");
    emit_byte(OP_RETURN);
  }
}

static void while_statement()
{
  int loop_start = current_chunk()->count;
  consume(TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
  expression();
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

  int exit_jump = emit_jump(OP_JUMP_IF_FALSE);
  emit_byte(OP_POP);
  statement();
  emit_loop(loop_start);

  patch_jump(exit_jump);
  emit_byte(OP_POP);
}

static void synchronize()
{
  parser.panic_mode = false;
  while (parser.current.type != TOKEN_EOF)
  {
    if (parser.previous.type == TOKEN_SEMICOLON) { return; }
    switch (parser.current.type)
    {
      case TOKEN_CLASS:
      case TOKEN_FUN:
      case TOKEN_VAR:
      case TOKEN_FOR:
      case TOKEN_IF:
      case TOKEN_WHILE:
      case TOKEN_PRINT:
      case TOKEN_RETURN:
        return ;

      default : ; // do nothing ... 
    }

    advance();
  }
}

static void declaration()
{
  if (match(TOKEN_FUN))
  {
    fun_declaration();
  }
  else if (match(TOKEN_VAR))
  {
    var_declaration();
  }
  else 
  {
    statement();
  }
  if (parser.panic_mode)
  {
    synchronize();
  }
}

static void statement()
{
  if (match(TOKEN_PRINT))
  {
    print_statement();
  }
  else if (match(TOKEN_FOR))
  {
    for_statement();
  }
  else if (match(TOKEN_IF))
  {
    if_statement();
  }
  else if (match(TOKEN_RETURN))
  {
    return_statement();
  }
  else if (match(TOKEN_WHILE))
  {
    while_statement();
  }
  else if (match(TOKEN_LEFT_BRACE))
  {
    begin_scope();
    block();
    end_scope();
  }
  else 
  {
    expression_statement();
  }
}

obj_function* compile(const char* source)
{
  init_scanner(source);
  compiler cmplr; 
  init_compiler(&cmplr, TYPE_SCRIPT);

  parser.had_error = false;
  parser.panic_mode = false;

  advance();
  while (!match(TOKEN_EOF))
  {
    declaration();
  }

  obj_function* func = end_compiler();

  return parser.had_error ? NULL : func;
}
