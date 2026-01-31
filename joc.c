#include <stddef.h>
#include <stdint.h>
#include <string.h>

// clang-format off
static inline int is_digit(const char *c) { return *c >= '0' && *c <= '9'; }
static inline int is_alpha(const char *c) { return (*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z'); }
static inline int is_alphanum(const char *c) { return is_digit(c) || is_alpha(c); }
static inline int is_whitespace(const char *c) { return *c == ' ' || *c == '\t' || *c == '\n'; }
static inline int is_control(const char *c) { return (*c < ' ' || *c == 0x7F) && !is_whitespace(c); }
static inline int is_newline(const char *c) { return *c == '\n'; }

enum TokenType {
  TOKEN_NONE,
  TOKEN_ID,  // sequence of alphabet (multople)
  TOKEN_NUM, // sequence of digits (multiple)
  TOKEN_SYM, // any symbol single only (single)
  TOKEN_SEP, // sequence of whitespace of same type, flattened (single)

  //phase2
  TOKEN_ROOT,
  TOKEN_KEY,
  TOKEN_BLOCK, //{
  TOKEN_BOX, //[
  TOKEN_PAREN, //(
  TOKEN_INTERP, //$
  TOKEN_ESCAPE, //temporary
  TOKEN_STR1, // INTERPOLATABLE
  TOKEN_STR2, // NON INTERPOLATABLE
  TOKEN_SLUG,
  TOKEN_COMMENT1, // //
  TOKEN_COMMENT2, // /*
};
typedef enum TokenType TokenType;

static inline TokenType char_code(char c) {
    return is_whitespace(&c) ? TOKEN_SEP :
           is_alpha(&c)      ? TOKEN_ID  :
           is_digit(&c)      ? TOKEN_NUM :
                               TOKEN_SYM;
}

// clang-format on

typedef struct {
  const char *src;
  int pos, line, col, len;
} Tokenizer;

typedef struct Token {
  enum TokenType type;
  const char *val;
  int len;
  int pos, line, col;

  // phase 2
  struct Token *parent;
  union {
    struct Token *next; // next sibling
    struct Token *last_child;
  };

  // phase 3
  const struct Token **child;
} Token;

static inline void tokenizer_init(Tokenizer *t, const char *src, size_t len) {
  t->src = src;
  t->pos = 0;
  t->line = 1;
  t->col = 0;
  t->len = len;
}

static inline void tokenizer_advance(Tokenizer *t) {
  t->pos++;
  t->col++;
  if (is_newline(&t->src[t->pos - 1])) {
    t->line++;
    t->col = 0;
  }
}

int token(Tokenizer *tok, Token *out) {
  TokenType prev = 0;

  int mpos = tok->pos, mcol = tok->col, mline = tok->line;
  int start = tok->pos;

  enum { NONE = 0, ADVANCE = 1, EMIT = 2, EMIT_ADVANCE = 3 };

  while (tok->pos < tok->len) {
    char c = tok->src[tok->pos];
    TokenType code = char_code(c);
    int todo = NONE;

    switch (code) {
    case TOKEN_SYM:
      if (tok->pos > start)
        todo = EMIT;
      else {
        todo = EMIT_ADVANCE;
        prev = code;
      }
      break;

    case TOKEN_ID:
    case TOKEN_NUM:
      if (code != prev && prev)
        todo = EMIT;
      else
        todo = ADVANCE;
      break;

    case TOKEN_SEP:
      if (tok->pos > start && tok->src[tok->pos - 1] != c)
        todo = EMIT;
      else
        todo = ADVANCE;
      break;

    default:
      return 0;
    }

    if (todo & ADVANCE)
      tokenizer_advance(tok);

    if (todo & EMIT) {
      // clang-format off
      *out = (Token){.type = prev, .val = tok->src + start, .len = tok->pos - start, .pos = mpos, .line = mline, .col = mcol};
      // clang-format on
      return 1;
    }

    prev = code;
  }

  if (prev) {
    // clang-format off
    *out = (Token){.type = prev, .val = tok->src + start, .len = tok->pos - start, .pos = mpos, .line = mline, .col = mcol};
    // clang-format on
    return 1;
  }

  return 0;
}

size_t tokens(Tokenizer *tok, Token *out, size_t cap) {
  size_t n = 0;
  while (n < cap && token(tok, out)) {
    out++;
    n++;
  }
  return n; // end pointer
}

enum Keyword {
  KEY_NONE,
  KEY_IF,
  KEY_ELSE,
  KEY_WHILE,
  KEY_DO,
  KEY_RETURN,
  KEY_BREAK,
  KEY_CONTINUE,
};

static inline enum Keyword get_keyword(const char *c) {
  if (!strcmp(c, "if"))
    return KEY_IF;
  if (!strcmp(c, "else"))
    return KEY_ELSE;
  if (!strcmp(c, "while"))
    return KEY_WHILE;
  if (!strcmp(c, "do"))
    return KEY_DO;
  if (!strcmp(c, "return"))
    return KEY_RETURN;
  if (!strcmp(c, "break"))
    return KEY_BREAK;
  if (!strcmp(c, "continue"))
    return KEY_CONTINUE;
  return KEY_NONE;
}

static inline void sum_token(Token *a, Token *b) {
  a->len += b->len;
  *b = (Token){};
}

void synize(Token *tokens, size_t tokens_len) {
  Token *stack[200];
  Token root = {.type = TOKEN_ROOT};
  size_t stack_len = 1;
  stack[0] = &root;

  enum act { do_push = 1, do_pop, do_upgrade, do_escape, do_append } act;
  enum TokenType payload;

  int pos = 0;
  while (pos < tokens_len) {
    Token *t = &tokens[pos++];
    Token *cur = stack[stack_len - 1];

    int in_string = cur->type == TOKEN_STR1 || cur->type == TOKEN_STR2;
    int in_comment = cur->type == TOKEN_COMMENT1 || cur->type == TOKEN_COMMENT2;
    int in_slug = in_string || in_comment;
    int in_interp = cur->type == TOKEN_INTERP;
    int in_atom = cur->type == TOKEN_ID || cur->type == TOKEN_NUM;
    char escape = cur->type == TOKEN_ESCAPE ? cur->val[0] : 0;

    // simple parenting
    t->parent = cur;
    if (t->parent->last_child)
      t->parent->last_child->next = t;
    else
      t->parent->last_child = t;

    payload = 0;
    act = 0;

    // TODO remove all 'do_xxx' and use only 'act' and 'payload'

    int do_push = TOKEN_NONE;
    int do_upgrade = TOKEN_NONE; // upgrade current's type
    int do_pop = 0;
    int do_normal = 0; // fallback
    int do_remove = 0; // kill this
    int do_escape = 0; // escape token
    int do_append = 0; // kill this and append to current
    int do_repeat = 0;

    switch (t->type) {

    case TOKEN_ID:
      if (!in_slug)
        do_push = t->type;
      else if (in_interp)
        do_upgrade = t->type;
      else if (escape == '.')
        do_upgrade = t->type;
      else
        do_normal = 1;
      break;

    case TOKEN_NUM:
      if (!in_slug)
        do_push = t->type;
      else if (escape == '.')
        do_upgrade = t->type;
      else
        do_normal = 1;
      break;

    case TOKEN_SYM:
      switch (t->val[0]) {
      case '.':
        if (cur->type == TOKEN_NUM || cur->type == TOKEN_ID)
          do_append = 1;
        else if (!in_slug)
          do_escape = 1;
        else
          do_normal = 1;
        break;
      case '{':
        if (!in_slug)
          do_push = TOKEN_BLOCK;
        else if (cur->type == TOKEN_INTERP) // ${
          do_upgrade = TOKEN_BLOCK;
        else
          do_normal = 1;
        break;
      case '(':
        if (!in_slug)
          do_push = TOKEN_PAREN;
        else
          do_normal = 1;
        break;
      case '[':
        if (!in_slug)
          do_push = TOKEN_BOX;
        else
          do_normal = 1;
        break;
      case '}':
        if (cur->type == TOKEN_BLOCK)
          do_pop = 1;
        else
          do_normal = 1;
        break;
      case ']':
        if (cur->type == TOKEN_BOX)
          do_pop = 1;
        else
          do_normal = 1;
        break;
      case ')':
        if (cur->type == TOKEN_PAREN)
          do_pop = 1;
        else
          do_normal = 1;
        break;
      case '$':
        if (!in_comment || cur->type == TOKEN_STR1)
          do_push = TOKEN_INTERP; // interp anything
        break;
      case '\'':
        if (in_string) // end string
          do_pop = 1;
        else if (!in_slug) {
          if (cur->type == TOKEN_INTERP)
            do_upgrade = TOKEN_STR2; //$''
          else
            do_push = TOKEN_STR1; // begin string
        }
        break;
      case '/':
        if (escape == '/') // begin comment //
          do_upgrade = TOKEN_COMMENT1;
        if (escape == '*' && cur->type == TOKEN_COMMENT2) // end */
          do_pop = 1;
        else
          do_push = TOKEN_ESCAPE;
        break;
      case '*':
        if (escape == '/') // begin comment */
          do_upgrade = TOKEN_COMMENT2;
        else if (cur->type == TOKEN_COMMENT2) // escape inside *
          do_push = TOKEN_ESCAPE;
        break;
      }

      break;

    case TOKEN_SEP:
      if (cur->type == TOKEN_ESCAPE || in_atom)
        do_pop = 1;
      else if (cur->type == TOKEN_COMMENT1 || is_newline(cur->val))
        do_pop = 1;

      break;
    }

    if (do_push) {
      stack[stack_len++] = t;
      t->type = do_push;
    } else if (do_upgrade) {
      cur->len += t->len;
      cur->type = do_upgrade;
      t->type = TOKEN_NONE;
      t->len = 0;
    } else if (do_pop) {
      stack_len--;
      t->type = TOKEN_NONE;
      if (cur->type == TOKEN_ID) {
        int key = get_keyword(t->val);
        if (key)
          cur->type = TOKEN_KEY;
      }
    } else if (do_normal) {
      if (in_slug)
        t->type = TOKEN_SLUG;
    } else if (do_remove) {
      t->type = TOKEN_NONE;
      t->len = 0;
    } else if (do_escape)
      t->type = TOKEN_ESCAPE;
    else if (do_append) {
      cur->len += t->len;
      t->type = TOKEN_NONE;
      t->len = 0;
    }
  }
}

// clang-format off
typedef enum { SEM_NONE, SEM_ID, SEM_WRAP, SEM_FUNC, SEM_CALL, SEM_NAMESPACE, SEM_BIN, SEM_FLOW, SEM_CONTROL, SEM_BLOCK, SEM_PAIR, SEM_DECL, SEM_ASSIGN } SemType;
typedef enum { MOD_NONE, MOD_REF, MOD_REX, MOD_NEW, MOD_OPT, MOD_MOV, MOD_NOT, MOD_NOX, MOD_NEG, MOD_POS } SemMod;

typedef struct Node Node;

struct Node {
  SemType t;
  const Token* token; //nullable

  union {
    struct { const char *name; unsigned short len; unsigned char mut; } id;
    struct { Node *x,*y; SemMod mod; unsigned pre:1; } wrap;
    struct { Node **args; int args_len; Node *ret_type; Node **body; int body_len; } func;
    struct { Node *func; Node **args; int args_len; } call;
    struct { const char *name; Node **body; int body_len; } ns;
    struct { const char *op; Node *a,*b; } bin;
    struct { enum Keyword kw; Node *subject; Node **body; int body_len; } flow;
    struct { enum Keyword kw; Node *payload; } control;
    struct { Node *what; Node **body; int body_len; } block;
    struct { Node *a,*b; } pair;
    struct { Node *name,*type,*init; } decl;
    struct { Node *name,*val; } assign;
  };
};

#undef SIM
#ifdef SIM
enum Specs {
	SP_NONE = 0,
	SP_NUMERIC = 1 << 0,
	SP_INTEGER = 1 << 1,
	SP_FLOATING = 1 << 2,
	SP_BIT8 = 1 << 3,
	SP_BIT16 = 1 << 4,
	SP_BIT32 = 1 << 5,
	SP_BIT64 = 1 << 6,
	SP_PRIMTIVE = 1 << 7, // numbers
	SP_MATHY = 1 << 8, // all are numbers only
	SP_PURE = 1 << 9, // value only, no refs
	SP_REF = 1 << 10, //this is a ref
	SP_MUT = 1 << 11, //this is a mutable ref

};

typedef struct Sim Sim;
struct Sim {
};

typedef  char* Name;
typedef struct Rule Rule;

struct Rule {
	int tag;
	union {
		enum Specs specs;
	};
};


Name sim_add_name(Sim* s, const char* n);
void sim_add_rule(Sim *s, const Name name, const Rule rule);

void sim_default(Sim * s) {
	Name n_int = sim_add_name(s, "int");
	Name n_float = sim_add_name(s, "float");

	const long prime = SP_MATHY | SP_PURE | SP_PRIMTIVE;
	sim_add_rule(s, n_int, (Rule){.specs = SP_INTEGER | SP_BIT32 | prime});
	sim_add_rule(s, n_float, (Rule){.specs = SP_FLOATING | SP_BIT64 | prime});
}
#endif
// clang-format on

#define TEST
#ifdef TEST
#include <stdio.h>

// clang-format off

static const char *token_type_name(enum TokenType t) {
  switch (t) {
  case TOKEN_NONE:     return "NONE";
  case TOKEN_ID:       return "ID";
  case TOKEN_NUM:      return "NUM";
  case TOKEN_SYM:      return "SYM";
  case TOKEN_SEP:      return "SEP";

  case TOKEN_ROOT:     return "ROOT";
  case TOKEN_KEY:      return "KEY";
  case TOKEN_BLOCK:    return "BLOCK";
  case TOKEN_BOX:      return "BOX";
  case TOKEN_PAREN:    return "PAREN";
  case TOKEN_INTERP:   return "INTERP";
  case TOKEN_ESCAPE:   return "ESC";
  case TOKEN_STR1:     return "STR";
  case TOKEN_STR2:     return "STR$";
  case TOKEN_SLUG:     return "SLUG";
  case TOKEN_COMMENT1: return "//";
  case TOKEN_COMMENT2: return "/*";

  default:             return "???";
  }
}
// clang-format on

void print_token(const Token *t) {
  if (!t->type)
    return;
  int depth = 0;
  Token *par = t->parent;
  while (par) {
    fputs("  ", stdout);
    par = par->parent;
  }
  printf("%-3s @%d:%d \"", token_type_name(t->type), t->line, t->col);

  fwrite(t->val, 1, t->len, stdout);
  puts("\"");
}

int main() {

  Tokenizer tok;
  const char src[] = "foo(){x} ";
  tokenizer_init(&tok, src, sizeof(src) - 1);

  static Token list[1000 * 1000];

  size_t len = tokens(&tok, list, 1000 * 1000);
  synize(list, len);

  for (int i = 0; i < len; i++) {
    print_token(&list[i]);
  }
}
#endif