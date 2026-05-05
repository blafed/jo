#include <stddef.h>
#include <stdint.h>

// NO NEED FOR C VM
// WE EXECUTE THE JO CODE DIRECTLY

void *jo_new(size_t size);             // heap alloc
void *jo_nez(size_t size);             // heap alloc and zero
void jo_del(void *ptr);                // heap free
void *jo_mov(void **ptr);              // heap move, invalidate ptr
void *jo_exp(void **ptr, size_t size); // mov + heap expand/realloc
void jo_cpy(void *dst, void *src, size_t len);
void *jo_ref(void *ptr); // can be implemented as just return the ptr
void jo_unref(void *ptr);
void jo_mmm(void *dst, void *src, size_t len); // memmove
void jo_mms(void *dst, void *src, size_t len); // memset

size_t jo_argc();
const char *jo_arg(int i);
void jo_echo(const char *msg, size_t len); // no format
void jo_abort();

int jo_io_open(const char *path, int flags, int mode);
int jo_io_close(int fd);
size_t jo_io_read(int fd, void *buf, size_t n);
size_t jo_io_write(int fd, const void *buf, size_t n);
long jo_io_seek(int fd, long offset, int origin);
void jo_exec(const char *cmd);

enum TokenType : char {
  JO_TOKEN_NONE,
  JO_TOKEN_ID,  // sequence of alphabet (multiple)
  JO_TOKEN_NUM, // sequence of digits (multiple)
  JO_TOKEN_SYM, // any symbol single only (single)
  JO_TOKEN_SEP, // sequence of whitespace of same type, flattened (single)

  // phase2
  JO_TOKEN_INTERP, // begins with $ inside string
  JO_TOKEN_DOLLAR, // begins with $ outside string
  JO_TOKEN_ROOT,
  JO_TOKEN_KEY,
  JO_TOKEN_GROUP,  //{} [] () <>
  JO_TOKEN_CURRY,  // if x
  JO_TOKEN_BLOCK,  //{
  JO_TOKEN_BOX,    //[
  JO_TOKEN_PAREN,  //(
  JO_TOKEN_ESCAPE, // temporary
  JO_TOKEN_STR1,   // '
  JO_TOKEN_STR2,   // "
  JO_TOKEN_SLUG,
  JO_TOKEN_COMMENT1, // //
  JO_TOKEN_COMMENT2, // /*
};

static const unsigned char JO_CHAR_TYPE[256] = {
#define SEP(x) [(unsigned char)(x)] = JO_TOKEN_SEP
#define NUM(x) [(unsigned char)(x)] = JO_TOKEN_NUM
#define ID(x) [(unsigned char)(x)] = JO_TOKEN_ID
#define SYM(x) [(unsigned char)(x)] = JO_TOKEN_SYM

    SEP(' '), SEP('\t'), SEP('\n'), SEP('\r'), SEP('\f'), SEP('\v'),

    NUM('0'), NUM('1'),  NUM('2'),  NUM('3'),  NUM('4'),  NUM('5'),  NUM('6'),
    NUM('7'), NUM('8'),  NUM('9'),

    ID('a'),  ID('b'),   ID('c'),   ID('d'),   ID('e'),   ID('f'),   ID('g'),
    ID('h'),  ID('i'),   ID('j'),   ID('k'),   ID('l'),   ID('m'),   ID('n'),
    ID('o'),  ID('p'),   ID('q'),   ID('r'),   ID('s'),   ID('t'),   ID('u'),
    ID('v'),  ID('w'),   ID('x'),   ID('y'),   ID('z'),

    ID('A'),  ID('B'),   ID('C'),   ID('D'),   ID('E'),   ID('F'),   ID('G'),
    ID('H'),  ID('I'),   ID('J'),   ID('K'),   ID('L'),   ID('M'),   ID('N'),
    ID('O'),  ID('P'),   ID('Q'),   ID('R'),   ID('S'),   ID('T'),   ID('U'),
    ID('V'),  ID('W'),   ID('X'),   ID('Y'),   ID('Z'),

    SYM('!'), SYM('"'),  SYM('#'),  SYM('$'),  SYM('%'),  SYM('&'),  SYM('\''),
    SYM('('), SYM(')'),  SYM('*'),  SYM('+'),  SYM(','),  SYM('-'),  SYM('.'),
    SYM('/'),

    SYM(':'), SYM(';'),  SYM('<'),  SYM('='),  SYM('>'),  SYM('?'),  SYM('@'),

    SYM('['), SYM('\\'), SYM(']'),  SYM('^'),  SYM('_'),  SYM('`'),

    SYM('{'), SYM('|'),  SYM('}'),  SYM('~'),

#undef SEP
#undef NUM
#undef ID
#undef SYM
};

static const char JO_CHAR_PAIR[] = {
    ['{'] = '}', ['('] = ')', ['<'] = '>', ['['] = ']',
    ['}'] = '{', [')'] = '(', ['>'] = '<', [']'] = '[',
};

// clang-format off
static const char *token_type_name(enum TokenType t) {
  switch (t) {
  case JO_TOKEN_NONE:     return "NONE";
  case JO_TOKEN_ID:       return "ID";
  case JO_TOKEN_NUM:      return "NUM";
  case JO_TOKEN_SYM:      return "SYM";
  case JO_TOKEN_SEP:      return "SEP";

  case JO_TOKEN_GROUP:    return "GROUP";
  case JO_TOKEN_INTERP:   return "INTERP";
  case JO_TOKEN_ROOT:     return "ROOT";
  case JO_TOKEN_KEY:      return "KEY";
  case JO_TOKEN_BLOCK:    return "BLOCK";
  case JO_TOKEN_BOX:      return "BOX";
  case JO_TOKEN_PAREN:    return "PAREN";
  case JO_TOKEN_ESCAPE:   return "ESC";
  case JO_TOKEN_STR1:     return "STR";
  case JO_TOKEN_STR2:     return "STR2";
  case JO_TOKEN_SLUG:     return "SLUG";
  case JO_TOKEN_COMMENT1: return "//";
  case JO_TOKEN_COMMENT2: return "/*";

  default:             return "???";
  }
}
// clang-format on

enum Keyword : unsigned char {
  JO_KEY_NONE,
  JO_KEY_IF,
  JO_KEY_ELSE,
  JO_KEY_WHILE,
  JO_KEY_DO,
  JO_KEY_RETURN,
  JO_KEY_BREAK,
  JO_KEY_CONTINUE,
  JO_KEY_WHEN,

  JO_KEY_IN,
  JO_KEY_OF,
  JO_KEY_STRUCT,
  JO_KEY_ENUM,
  JO_KEY_ALIAS,
  JO_KEY_THIS,

  JO_KEY_TRUE,
  JO_KEY_FALSE,
  JO_KEY_NULL,
};

enum Symbol : unsigned char {
  SYM_NONE = 0,

  SYM_EXCL = '!',
  SYM_DQUOTE = '"',
  SYM_HASH = '#',
  SYM_DOLLAR = '$',
  SYM_PERCENT = '%',

  SYM_AMP = '&',
  SYM_SQUOTE = '\'',
  SYM_LPAREN = '(',
  SYM_RPAREN = ')',
  SYM_STAR = '*',

  SYM_PLUS = '+',
  SYM_COMMA = ',',
  SYM_MINUS = '-',
  SYM_DOT = '.',
  SYM_SLASH = '/',

  SYM_COLON = ':',
  SYM_SEMI = ';',
  SYM_LT = '<',
  SYM_EQ = '=',
  SYM_GT = '>',

  SYM_QUESTION = '?',
  SYM_AT = '@',

  SYM_LBRACKET = '[',
  SYM_BACKSLASH = '\\',
  SYM_RBRACKET = ']',
  SYM_CARET = '^',
  SYM_UNDERSCORE = '_',
  SYM_BACKTICK = '`',

  SYM_LBRACE = '{',
  SYM_PIPE = '|',
  SYM_RBRACE = '}',
  SYM_TILDE = '~',

  SYMMANY = 128,

  SYM_DECL,           // :=
  SYM_NULLER,         // ??
  SYM_INC,            // ++
  SYM_DEC,            // --
  SYM_SHL,            // <<
  SYM_SHR,            // >>
  SYM_LOR,            // ||
  SYM_LAND,           // &&
  SYM_DOTDOT,         // ..
  SYM_ADD_EQ,         // +=
  SYM_SUB_EQ,         // -=
  SYM_MUL_EQ,         // *=
  SYM_DIV_EQ,         // /=
  SYM_MOD_EQ,         // %=
  SYM_XOR_EQ,         // ^=
  SYM_AND_EQ,         // &=
  SYM_OR_EQ,          // |=
  SYM_EQEQ,           // ==
  SYM_NEQ,            // !=
  SYM_LTE,            // <=
  SYM_GTE,            // >=
  SYM_ARROW,          // ->
  SYM_FATARROW,       // =>
  SYM_SCOPE,          // ::
  SYM_COMMENT1,       // //
  SYM_COMMENT2_BEGIN, // /*
  SYM_COMMENT2_END,   // */

  SYM_REF_EQ,        // &==
  SYM_SHL_EQ,        // <<=
  SYM_SHR_EQ,        // >>=
  SYM_TRIPLE_DQUOTE, // """
  SYM_TRIPLE_SQUOTE, // '''
  SYM_TODO           // ???

};

struct jo_ast {};
struct jo_token {
  uint8_t type, data;
  int pos, line, col, len;
  const char *val;
  int depth;
};
struct jo_cursor {
  int pos, line, col;
};

typedef struct jo_token jo_token;
typedef struct jo_ast jo_ast;
typedef struct jo_cursor jo_cursor;

// clang-format off
// the text SHOULD be null terminated
int jo_tok(const char *src, struct jo_cursor *tok, struct jo_token *out) {
  out->pos = tok->pos; out->line = tok->line; out->col = tok->col;
  int start = tok->pos;

  uint8_t past = 0;

  while (1) {
    char c = src[tok->pos];
    uint8_t code = JO_CHAR_TYPE[(unsigned char)c];

    char emit = 0;

    switch (code) {
    case JO_TOKEN_NONE:
      if(past) {out->type = past; out->val = src + start; out->len = tok->pos - start; return 1;}
      return 0;
      break;
    case JO_TOKEN_ID:
    case JO_TOKEN_NUM:
      if (past != code) emit = 1;
      break;
    case JO_TOKEN_SEP:
      if (past != code || (tok->pos && src[tok->pos - 1] != c)) emit = 1;
      break;
    case JO_TOKEN_SYM: out->data = c; emit = 1; break;
    default: emit = 1; break;
    }

    if (emit && past) {
      out->type = past; out->val = src + start; out->len = tok->pos - start;
      return 1;
    }

    tok->pos++;
    tok->col++;
    if (c == '\n') {
      tok->line++;
      tok->col = 0;
    }
    past = code;
  }
  *out = (struct jo_token){};

  return 0;
}
// clang-format on

// clang-format off

//
//=======================================
//

// clang-format on
#include <stdio.h>

void print_token(const struct jo_token *t) {
  if (!t->type)
    return;

  // indentation via depth
  for (int i = 0; i < t->depth; i++)
    fputs(" |", stdout);

  printf("%-3s @%d:%d ", token_type_name(t->type), t->line, t->col);

  // print raw slice (not null-terminated)
  putchar('"');
  fwrite(t->val, 1, t->len, stdout);
  putchar('"');

  if (t->data)
    printf(" (%u)", t->data);

  putchar('\n');
}

int main() {
  const char *input = "\b\n";
  jo_cursor cursor = {};
  jo_token token = {};
  while (jo_tok(input, &cursor, &token))
    print_token(&token);
  printf("\n");
  return 0;
}
