#include <stddef.h>
#include <stdint.h>

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
  int pos, line, col, len;

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

struct Syn {};

typedef struct Syn Syn;

typedef enum {
  SYN_SEP = -1,
  SYN_ATOM,
  SYN_SEQ,
  SYN_GROUP,
  SYN_SLUG,
  SYN_STRING,
  SYN_COMMENT
} SynType;

typedef struct Node Node;
struct Node {
  Node *parent;
  Node **child;
  SynType type;
  const char *val;
  Token token;
};

Node synize() {}

#define TEST
#ifdef TEST
#include <stdio.h>

static const char *token_type_name(enum TokenType t) {
  switch (t) {
  case TOKEN_NONE:
    return "NONE";
  case TOKEN_ID:
    return "ID";
  case TOKEN_NUM:
    return "NUM";
  case TOKEN_SYM:
    return "SYM";
  case TOKEN_SEP:
    return "SEP";
  default:
    return "???";
  }
}

void print_token(const Token *t) {
  printf("%-3s @%d:%d \"", token_type_name(t->type), t->line, t->col);

  fwrite(t->val, 1, t->len, stdout);
  puts("\"");
}

int main() {

  Tokenizer tok;
  const char src[] = "foo() {}";
  tokenizer_init(&tok, src, sizeof(src) - 1);

  static Token list[1000 * 1000];

  size_t len = tokens(&tok, list, 1000 * 1000);

  for (int i = 0; i < len; i++) {
    print_token(&list[i]);
  }
}
#endif