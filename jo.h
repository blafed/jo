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

void jo_stry_int(long i, char *buf);
void jo_stry_uint(unsigned long i, char *buf);
void jo_stry_float(double f, char *buf);
void jo_stry_bool(int b, char *buf);

int jo_parse_uint(unsigned long *out, const char *s, size_t n, int base);
int jo_parse_float(double *out, const char *s, size_t n, int base);

char jo_parse_int_full(long *i, const char *buf, size_t len);
char jo_parse_uint_full(unsigned long *i, const char *buf, size_t len);
char jo_parse_float_full(long double *i, const char *buf, size_t len);
char jo_parse_bool_full(int *i, const char *buf, size_t len);

enum jo_token_type : char {
  JO_TOKEN_NONE,
  JO_TOKEN_ID,    // sequence of alphabet (multiple)
  JO_TOKEN_NUM,   // sequence of digits (multiple)
  JO_TOKEN_SYM,   // any symbol single only (single) (multiple when meaning)
  JO_TOKEN_SEP,   // sequence of whitespace of same type, flattened (single)
  JO_TOKEN_EMPTY, // deleted token

  // phase2
  JO_TOKEN_ROOT,
  JO_TOKEN_INTERP, // begins with $ inside string
  JO_TOKEN_GROUP,  //{} [] () <>
  JO_TOKEN_ESCAPE, //
  JO_TOKEN_STR1,   // '
  JO_TOKEN_STR2,   // "
  JO_TOKEN_SLUG,
  JO_TOKEN_COMMENT1, // //
  JO_TOKEN_COMMENT2, // /*
  JO_TOKEN_KEYWORD,
  JO_TOKEN_TERM,
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

static const unsigned char JO_CHAR_ESCAPE[256] = {
    ['\''] = '\'', ['"'] = '"',  ['\\'] = '\\', ['n'] = '\n',
    ['r'] = '\r',  ['t'] = '\t', ['b'] = '\b',  ['f'] = '\f',
    ['v'] = '\v',  ['a'] = '\a', ['0'] = '\0'};

// clang-format off
static const char *token_type_name(enum jo_token_type t) {
  static char stuff[100] = "???";
  switch (t) {
  case JO_TOKEN_NONE:     return "NONE";
  case JO_TOKEN_ID:       return "ID";
  case JO_TOKEN_NUM:      return "NUM";
  case JO_TOKEN_SYM:      return "SYM";
  case JO_TOKEN_SEP:      return "SEP";

  case JO_TOKEN_GROUP:    return "GROUP";
  case JO_TOKEN_INTERP:   return "INTERP";
  case JO_TOKEN_ESCAPE:   return "ESC";
  case JO_TOKEN_STR1:     return "STR";
  case JO_TOKEN_STR2:     return "STR2";
  case JO_TOKEN_SLUG:     return "SLUG";
  case JO_TOKEN_COMMENT1: return "//";
  case JO_TOKEN_COMMENT2: return "/*";

  default:jo_stry_uint(t, stuff + 3); return stuff;
  }
}
// clang-format on

// enum Keyword : unsigned char {
//   JO_KEY_NONE,
//   JO_KEY_IF,
//   JO_KEY_ELSE,
//   JO_KEY_WHILE,
//   JO_KEY_DO,
//   JO_KEY_RETURN,
//   JO_KEY_BREAK,
//   JO_KEY_CONTINUE,
//   JO_KEY_WHEN,

//   JO_KEY_IN,
//   JO_KEY_OF,
//   JO_KEY_STRUCT,
//   JO_KEY_ENUM,
//   JO_KEY_ALIAS,
//   JO_KEY_THIS,

//   JO_KEY_TRUE,
//   JO_KEY_FALSE,
//   JO_KEY_NULL,
// };

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

  SYM_REF_EQ,        // ===
  SYM_SHL_EQ,        // <<=
  SYM_SHR_EQ,        // >>=
  SYM_TRIPLE_DQUOTE, // """
  SYM_TRIPLE_SQUOTE, // '''
  SYM_TODO           // ???

};

struct jo_token {
  uint8_t type, data;
  int pos, line, col, len;
  union {
    const char *val;
    uintptr_t ref;
  };
  int depth;
};
struct jo_cursor {
  int pos, line, col;
};

typedef struct jo_token jo_token;
typedef struct jo_ast jo_ast;
typedef struct jo_cursor jo_cursor;

// clang-format off
// the src should be NULL TERMINATED
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
      if(past) { out->type = past; out->val = src + start; out->len = tok->pos - start; out->data=out->val[0]; return 1;}
      return 0;
      break;
    case JO_TOKEN_ID:
    case JO_TOKEN_NUM:
      if (past != code) emit = 1;
      break;
    case JO_TOKEN_SEP:
      if (past != code || (tok->pos && src[tok->pos - 1] != c)) emit = 1;
      break;
    case JO_TOKEN_SYM: emit = 1; break;
    default: emit = 1; break;
    }

    if (emit && past) {
      out->type = past; out->val = src + start; out->len = tok->pos - start;
      out->data = out->val[0];
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


static inline enum Symbol jo_sym(const char* c, size_t len) {
    switch (len) {

		//TODO remove this?
    case 1:
        switch (c[0]) {
            case '!': return SYM_EXCL;
            case '"': return SYM_DQUOTE;
            case '#': return SYM_HASH;
            case '$': return SYM_DOLLAR;
            case '%': return SYM_PERCENT;
            case '&': return SYM_AMP;
            case '\'': return SYM_SQUOTE;
            case '(': return SYM_LPAREN;
            case ')': return SYM_RPAREN;
            case '*': return SYM_STAR;
            case '+': return SYM_PLUS;
            case ',': return SYM_COMMA;
            case '-': return SYM_MINUS;
            case '.': return SYM_DOT;
            case '/': return SYM_SLASH;
            case ':': return SYM_COLON;
            case ';': return SYM_SEMI;
            case '<': return SYM_LT;
            case '=': return SYM_EQ;
            case '>': return SYM_GT;
            case '?': return SYM_QUESTION;
            case '@': return SYM_AT;
            case '[': return SYM_LBRACKET;
            case '\\': return SYM_BACKSLASH;
            case ']': return SYM_RBRACKET;
            case '^': return SYM_CARET;
            case '_': return SYM_UNDERSCORE;
            case '`': return SYM_BACKTICK;
            case '{': return SYM_LBRACE;
            case '|': return SYM_PIPE;
            case '}': return SYM_RBRACE;
            case '~': return SYM_TILDE;
        }
        break;

    case 2:
        switch (c[0]) {
            case ':': if (c[1] == '=') return SYM_DECL; if(c[1] == ':') return SYM_SCOPE; break;
            case '?': if (c[1] == '?') return SYM_NULLER; break;
            case '+': if (c[1] == '+') return SYM_INC;
                      if (c[1] == '=') return SYM_ADD_EQ;
                      break;
            case '-': if (c[1] == '-') return SYM_DEC;
                      if (c[1] == '=') return SYM_SUB_EQ;
                      if (c[1] == '>') return SYM_ARROW;
                      break;
            case '<': if (c[1] == '<') return SYM_SHL;
                      if (c[1] == '=') return SYM_LTE;
                      break;
            case '>': if (c[1] == '>') return SYM_SHR;
                      if (c[1] == '=') return SYM_GTE;
                      break;
            case '|': if (c[1] == '|') return SYM_LOR;
                      if (c[1] == '=') return SYM_OR_EQ;
                      break;
            case '&': if (c[1] == '&') return SYM_LAND;
                      if (c[1] == '=') return SYM_AND_EQ;
                      break;
            case '.': if (c[1] == '.') return SYM_DOTDOT; break;
            case '*': if(c[1] == '/') return SYM_COMMENT2_END; if (c[1] == '=') return SYM_MUL_EQ; break;
        	case '/':
        	    if (c[1] == '=') return SYM_DIV_EQ;
        	    if (c[1] == '/') return SYM_COMMENT1;
        	    if (c[1] == '*') return SYM_COMMENT2_BEGIN;
        	    break;
            case '%': if (c[1] == '=') return SYM_MOD_EQ; break;
            case '^': if (c[1] == '=') return SYM_XOR_EQ; break;
            case '=': if (c[1] == '=') return SYM_EQEQ;
                      if (c[1] == '>') return SYM_FATARROW;
                      if (c[1] == '=' && c[2] == '=') return SYM_REF_EQ;
                      break;
            case '!': if (c[1] == '=') return SYM_NEQ; break;
        }
        break;

    case 3:
        switch (c[0]) {
            case '<': if (c[1] == '<' && c[2] == '=') return SYM_SHL_EQ; break;
            case '>': if (c[1] == '>' && c[2] == '=') return SYM_SHR_EQ; break;
            case '"': if (c[1] == '"' && c[2] == '"') return SYM_TRIPLE_DQUOTE; break;
            case '\'': if (c[1] == '\'' && c[2] == '\'') return SYM_TRIPLE_SQUOTE; break;
			case '?': if (c[1] == '?' && c[2] == '?') return SYM_TODO; break;
        }
        break;
    }

	return 0;
}

void jo_tok_sym(jo_token* tokens) {
	jo_token *stack[3];
	int sp = 0;

	jo_token *t = tokens;
	
	for(;; ++t) {
		int type = t->type;

		if(!type) break;
		else if(type == JO_TOKEN_SYM) stack[sp++] = t;
		else if(sp < 2) sp = 0;


		if(sp == 3) {
			unsigned char data = jo_sym(stack[0]->val, 3);
			if(data) {
				stack[0]->data = data;
				stack[0]->len = 3;
				stack[1]->type = JO_TOKEN_EMPTY;
				stack[2]->type = JO_TOKEN_EMPTY;
				sp = 0;
			} else {
				data = jo_sym(stack[0]->val, 2);
				if(data) {
					stack[0]->data = data;
					stack[0]->len = 2;
					stack[1]->type = JO_TOKEN_EMPTY;

					stack[0] = stack[2];
					sp = 1;
				} else {
					stack[0] = stack[1];
					stack[1] = stack[2];
					sp = 2;
				}
			}
		} else if(sp == 2 && type != JO_TOKEN_SYM) {
			unsigned char data = jo_sym(stack[0]->val, 2);
			if(data) {
				stack[0]->data = data;
				stack[0]->len = 2;
				stack[1]->type = JO_TOKEN_EMPTY;
			}
			sp = 0;
		}
	}
}

static jo_token JO_ROOT_TOKEN = {.type = JO_TOKEN_ROOT};

void jo_lex(jo_token* tokens, jo_token* stack[]) {

  #define in_string (cur->type == JO_TOKEN_STR1 || cur->type == JO_TOKEN_STR2)
	#define in_comment (cur->type == JO_TOKEN_COMMENT1 || cur->type == JO_TOKEN_COMMENT2)
	#define in_slug (in_string || in_comment)
	#define in_atom (cur->type == JO_TOKEN_ID || cur->type == JO_TOKEN_NUM)
	#define in_escape (cur->type == JO_TOKEN_ESCAPE)
	#define in_interp (cur->type == JO_TOKEN_INTERP)
	// #define in_dollar (cur->type == jo_token_interp)

  enum {_NOTHING, _PUSH, _POP, _UPGRADE, _ESC, _KILL, _APPEND, _TERM, _SLUGY, _REMOVE, _ERR };
  #define kill todo = _KILL;
	#define append todo = _APPEND;
	#define term todo = _TERM;
	#define slugy todo = _SLUGY;
	#define remove todo = _REMOVE;
	#define err todo = _ERR;
	#define push(x) todo = _PUSH, payload = x;
	#define pop todo = _POP;
  // #define mut(val) { todo = _MUT; payload = val; }
  #define escape todo = _ESC;
  #define keep ;
	// #define upgrade(x) todo = _UPGRADE, payload = x;

	jo_token* t = tokens;
	int sp = 0;
  stack[sp] = &JO_ROOT_TOKEN;
  
  for(;; t++) {
    int type = t->type;
    if (!type) break;
    // printf("sp %d, type %d %zu\n\n", sp, type, t - tokens);
    jo_token * cur = stack[sp];
    t->depth = cur->depth + 1;

    int todo = _NOTHING;
    enum jo_token_type payload;
    switch (t->type) {
      case JO_TOKEN_EMPTY: continue;
      case JO_TOKEN_ID:
      case JO_TOKEN_NUM:
        if (in_atom) append
        else if(in_escape) escape
        else if(in_slug) slugy
        else push(t->type)
        break;
      case JO_TOKEN_SEP:
        switch (t->val[0]) {
          case '\n':
            if (cur->type == JO_TOKEN_COMMENT1) pop
            else if(in_atom) term
            // else if(in_angle) kill
            else if(in_slug) slugy
            else if(in_interp) term
            else keep
          break;
          default:
            if(in_atom) term
            else if(in_slug) slugy
            else keep
            // else if(in_angle)
            break;
        }
        break;
        
      case JO_TOKEN_SYM:
        switch (t->data) {
          case '.':
          case '_':
            if (in_atom) append
            else if(in_slug) slugy
            else push(JO_TOKEN_ID)
            break;
          case '{': case '[': case '(':
            if (in_atom) term
            else if(in_interp) push(JO_TOKEN_GROUP)
            else if(in_slug) slugy
            else push(JO_TOKEN_GROUP)
            break;
          case '}': case ']': case ')':
            if(in_atom) term
            else if(cur->type == JO_TOKEN_GROUP && t->data == JO_CHAR_PAIR[cur->data]) pop
            else if(in_slug) slugy
            else err
            break;
          case '$':
            if(in_atom)term
            else if(in_escape) escape
            else if(in_comment) append 
            else push(JO_TOKEN_INTERP)
            break;
          case SYM_COMMENT1:
            if(in_atom) term
            else if (in_slug) slugy
            else push(JO_TOKEN_COMMENT1)
            break;
          case SYM_COMMENT2_BEGIN:
            if(in_atom) term
            else if (in_slug) slugy
            else push(JO_TOKEN_COMMENT2)
            break;
          case SYM_COMMENT2_END:
            if(in_atom) term
            else if(in_comment && cur->type == JO_TOKEN_COMMENT2) pop
            else if(in_slug) slugy
            else err
            break;
          case '\\':
            if(in_atom) term
            else if(in_escape) escape
            else push(JO_TOKEN_ESCAPE)
            break;
          case '\'':
            if(in_atom) term
            else if(cur->type == JO_TOKEN_STR1) pop
            else if(in_escape) escape
            else if(in_slug) slugy
            else push(JO_TOKEN_STR1)
            break;
          case '"':
            if(in_atom) term
            else if(cur->type == JO_TOKEN_STR2) pop
            else if(in_escape) escape
            else if(in_slug) slugy
            else push(JO_TOKEN_STR2)
            break;
          default:
            if(in_atom) term
            else if(in_slug) slugy
            else keep
            break;
        }
        break;
    }

    switch(todo) {
      case _APPEND:cur->len += t->len;t->type = 0;break;
      case _PUSH:stack[++sp]=t;t->type=payload;break;
      // case _UPGRADE:t->type=payload;stack[++sp]=t; break;
      case _ESC:cur->data = JO_CHAR_ESCAPE[t->data];sp--;t->type=0;cur->len++;break;
      case _POP:sp--;t->type = 0; break;
      case _SLUGY:t->type=JO_TOKEN_SLUG;break;
      case _REMOVE:t->type=0;break;
      case _TERM:t--;sp--;break;
      case _ERR: jo_echo("error parsing \n", 14); break;
    }

    if(sp < 0) {
      jo_echo("error negative\n", 30);
      return;
    }
  }
#undef in_string
#undef in_comment
#undef in_slug
#undef in_atom
#undef in_escape
#undef in_interp

#undef kill
#undef append
#undef term
#undef slugy
#undef remove
#undef err
#undef push
#undef pop
#undef upgrade
#undef escape
#undef keep
}


enum {
  JO_KW_IF = 1, JO_KW_ELSE, JO_KW_FOR, JO_KW_WHEN, 
  JO_KW_CONTINUE, JO_KW_BREAK, JO_KW_RETURN,
  JO_KW_OF, JO_KW_IN, JO_KW_IS,
  JO_KW_ME,
  JO_KW_ENUM, JO_KW_STRUCT, 
  JO_KW_REF,JO_KW_NEW,JO_KW_DEL, 
  JO_KW_VOID,
  JO_KW_ALIAS,
  JO_KW_MAX,

  JO_TP_STR,
  JO_TP_VOIDPTR, JO_TP_CHARPTR, JO_TP_USIZE,
  JO_TP_BYTE, JO_TP_SBYTE, JO_TP_SHORT, JO_TP_USHORT, JO_TP_INT, JO_TP_UINT,
  JO_TP_LONG, JO_TP_ULONG, JO_TP_FLOAT, JO_TP_DOUBLE,
  JO_VW_TRUE, JO_VW_FALSE, JO_VW_NULL,
  JO_TP_INFER,
};

#include <stdint.h>
#include <stddef.h>

uint8_t jo_id_data(const char* s, size_t n) {
    if (!s || !n) return 0;

    switch (s[0]) {
    case 'i':
        if (n==2 && s[1]=='f') return JO_KW_IF;
        if (n==3 && s[1]=='n' && s[2]=='t') return JO_TP_INT;
        break;

    case 'e':
        if (n==4 && s[1]=='l' && s[2]=='s' && s[3]=='e') return JO_KW_ELSE;
        if (n==4 && s[1]=='n' && s[2]=='u' && s[3]=='m') return JO_KW_ENUM;
        break;

    case 'f':
        if (n==3 && s[1]=='o' && s[2]=='r') return JO_KW_FOR;
        if (n==5 && s[1]=='l' && s[2]=='o' && s[3]=='a' && s[4]=='t') return JO_TP_FLOAT;
        break;

    case 'w':
        if (n==4 && s[1]=='h' && s[2]=='e' && s[3]=='n') return JO_KW_WHEN;
        break;

    case 'o':
        if (n==2 && s[1]=='f') return JO_KW_OF;
        break;

    case 'm':
        if (n==2 && s[1]=='e') return JO_KW_ME;
        break;

    case 'c':
        if (n==8 && s[1]=='o' && s[2]=='n' && s[3]=='t' && s[4]=='i' && s[5]=='n' && s[6]=='u' && s[7]=='e')
            return JO_KW_CONTINUE;
        if (n==7 && s[1]=='h' && s[2]=='a' && s[3]=='r' && s[4]=='p' && s[5]=='t' && s[6]=='r')
            return JO_TP_CHARPTR;
        break;

    case 'b':
        if (n==5 && s[1]=='r' && s[2]=='e' && s[3]=='a' && s[4]=='k') return JO_KW_BREAK;
        if (n==4 && s[1]=='y' && s[2]=='t' && s[3]=='e') return JO_TP_BYTE;
        break;

    case 'r':
        if (n==6 && s[1]=='e' && s[2]=='t' && s[3]=='u' && s[4]=='r' && s[5]=='n') return JO_KW_RETURN;
        if (n==3 && s[1]=='e' && s[2]=='f') return JO_KW_REF;
        break;

    case 's':
        if (n==3 && s[1]=='t' && s[2]=='r') return JO_TP_STR;
        if (n==5 && s[1]=='b' && s[2]=='y' && s[3]=='t' && s[4]=='e') return JO_TP_SBYTE;
        if (n==5 && s[1]=='h' && s[2]=='o' && s[3]=='r' && s[4]=='t') return JO_TP_SHORT;
        if (n==6 && s[1]=='t' && s[2]=='r' && s[3]=='u' && s[4]=='c' && s[5]=='t') return JO_KW_STRUCT;
        break;

    case 'u':
        if (n==6 && s[1]=='s' && s[2]=='h' && s[3]=='o' && s[4]=='r' && s[5]=='t') return JO_TP_USHORT;
        if (n==5 && s[1]=='s' && s[2]=='i' && s[3]=='z' && s[4]=='e') return JO_TP_USIZE;
        if (n==4 && s[1]=='i' && s[2]=='n' && s[3]=='t') return JO_TP_UINT;
        if (n==5 && s[1]=='l' && s[2]=='o' && s[3]=='n' && s[4]=='g') return JO_TP_ULONG;
        break;

    case 'l':
        if (n==4 && s[1]=='o' && s[2]=='n' && s[3]=='g') return JO_TP_LONG;
        break;

    case 'd':
        if (n==6 && s[1]=='o' && s[2]=='u' && s[3]=='b' && s[4]=='l' && s[5]=='e') return JO_TP_DOUBLE;
        if (n==3 && s[1]=='e' && s[2]=='l') return JO_KW_DEL;
        break;

    case 'n':
        if (n==3 && s[1]=='e' && s[2]=='w') return JO_KW_NEW;
        break;

    case 'v':
        if (n==4 && s[1]=='o' && s[2]=='i' && s[3]=='d') return JO_KW_VOID;
        if (n==7 && s[1]=='o' && s[2]=='i' && s[3]=='d' && s[4]=='p' && s[5]=='t' && s[6]=='r')
            return JO_TP_VOIDPTR;
        break;
    }

    return 0;
}

void jo_iden(jo_token* tokens) {
  jo_token* t = tokens;
  for(;;t++) {
    if (t->type == 0){
      break;
    }
    if(t->type == JO_TOKEN_ID) {
      t->data = jo_id_data(t->val, t->len);
      if(t->data < JO_KW_MAX)  t->type = JO_TOKEN_KEYWORD;
    }else if (t->type == JO_TOKEN_SEP && t->val[0] == '\n' || t->type == JO_TOKEN_SYM && t->data == ',') {
      t->type = JO_TOKEN_TERM;
    }
  }
}


////////////// AST

/*

1. followers (eg. of)
2. group -> pair usually
3. terminate on terminator

a b -> pair
a b c -> pair(a, pair(b, c))
foo()void -> pair(foo, pair((), void))
x struct {} -> pair(x, pair(struct, {})
if x y -> pair(if x, y)
if x y else z -> pair(if x, pair(y, else z))
for x in 0..10 x-> pair(bin(for x, in, 0..10), x)
a {} + x -> bin(pair(a, {}), x)
x + -y -> bin(x, +, un(y, -))
x = () {} -> bin(x, =, pair((), {}))
foo () 123 -> pair(foo, pair((), 123))
if y - 100 - 100 -> pair(pair(if, bin(bin(y - 100), -, 100)))

() {} wants its parent to be pair, not bin

if x y -> ((if x) y)
if x y {} -> ((if x) (y {}))
if x + y {} -> ((if (x + y)) {})
x = y {} -> (x = (y {}))
if x = y {} -> ((if (x = y)) {})
a-b bin
a -b pair(un)
a - b bin


the 3 primtives:
pair, bin, un

//order is
un, pair, bin


resolve:
turn asts into names
turn names into types

name -> {type, alias, path}
x alias y
z alias y
foo(x int) {
  if x == 0 {
    y 0
  }
}
foo
foo.x
foo.0.y


handle macros and name aliases

i32 # int
foo (x i32) {}
stuff {{foo, func}}

x {
}

TP handling
Unsuffixed literal → type of neighbor (if binary op) else int32
Binary a op b → result type = highest(a.type, b.type)

*/

enum {
  JO_REL_CONTAINER = 0, JO_REL_BIN = 1, JO_REL_UN, JO_REL_PAIR,
};

struct jo_rel {
  int depth;
  char type;
  jo_token* tok;
  size_t len;
};

typedef struct jo_rel jo_rel;

int jo_precedence(jo_token* t, int has_lhs, int has_rhs){
  if (!has_lhs)
    return 100;
  switch (t->type) {
    case JO_TOKEN_SYM:
      switch (t->data) {
        case '*': case '/': return 3;
        case '+': case '-': return has_lhs;
      }
      return t->data;
  }
  return -1;
}

// int jo_relate(jo_token* tokens, jo_rel *stack[], jo_rel* out) {
//   jo_rel root = {.tok = tokens};
//   int sp = 0;
//   stack[sp++] = &root;

//   enum {PUSH, POP, KILL, APPEND, TERM, SLUGY, REMOVE, ERR };
//   #define pair todo = PUSH;
//   #define bin todo = PUSH;
//   #define un todo = PUSH;
  



//   for(jo_token* t = tokens;;t++){
//     if (t->typ == 0) {
//       break;
//     }

//   }

// }

enum JO_AST_TYPE{
  JO_AST_BIN, JO_AST_UN,
  JO_AST_NUM, JO_AST_ID, JO_AST_STR,

  JO_AST_COMMAND,
};

struct jo_ast {
  int depth;
  enum JO_AST_TYPE type;
  union{
    struct {int a, b; uint8_t op;}bin;
    struct {int x; uint8_t op;}un;
    struct {int start, len;} block;
    struct {int fn, args; } call;
    struct {int a, b; } pair;
    uint8_t opcode;
    //atoms
    struct {const char* val; size_t len;} atom;
    struct {uint64_t val; size_t len; char tp;} num;
  };
};

int jo_asty(jo_token* tokens, jo_ast* out) {

  char want_term = 0;
  for(jo_token* t = tokens; ; t++){
    if (t->type == 0){
      break;
    }

    if (want_term && t->type == JO_TOKEN_TERM) {
      return 1;
    }

    switch (t->type) {
      case JO_TOKEN_ID:
        out->type = JO_AST_ID;
        out->atom.val = t->val;
        out->atom.len = t->len;
        want_term = 1;
        break;
      case JO_TOKEN_NUM:
        out->type = JO_AST_NUM;
        out->num.tp = jo_parse_uint_full(&out->num.val, t->val, t->len);
        break;
    }
  }
}


static inline int jo_digit_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

int jo_parse_uint(unsigned long *out, const char *s, size_t n, int base) {
    unsigned long v = 0;

    for (size_t i = 0; i < n; i++) {
        char c = s[i];
        if (c == '_') continue;

        int d = jo_digit_val(c);
        if (d < 0 || d >= base) return 0;

        v = v * base + d;
    }

    *out = v;
    return 1;
}

static uint8_t jo_suffix_tp(const char *s, size_t n) {
    // short ones
    if (n == 1) {
      switch (s[0]) {
        case 'b': return JO_TP_BYTE;
        case 's': return JO_TP_SHORT;
        case 'l': return JO_TP_LONG;
        case 'u': return JO_TP_UINT;
        case 'i': return JO_TP_INT;
        case 'f': return JO_TP_FLOAT;
        case 'd': return JO_TP_DOUBLE;
      }
    }

    if (n==2 && s[0]=='u' && s[1]=='l') return JO_TP_ULONG;
    if (n==2 && s[0]=='s' && s[1]=='b') return JO_TP_SBYTE;
    if (n==2 && s[0]=='u' && s[1]=='s') return JO_TP_USHORT;

    // sized
    if (n==2 && s[0]=='i' && s[1]=='8') return JO_TP_SBYTE;
    if (n==2 && s[0]=='u' && s[1]=='8') return JO_TP_BYTE;

    if (n==3 && s[0]=='i' && s[1]=='1' && s[2]=='6') return JO_TP_SHORT;
    if (n==3 && s[0]=='u' && s[1]=='1' && s[2]=='6') return JO_TP_USHORT;

    if (n==3 && s[0]=='i' && s[1]=='3' && s[2]=='2') return JO_TP_INT;
    if (n==3 && s[0]=='u' && s[1]=='3' && s[2]=='2') return JO_TP_UINT;

    if (n==3 && s[0]=='i' && s[1]=='6' && s[2]=='4') return JO_TP_LONG;
    if (n==3 && s[0]=='u' && s[1]=='6' && s[2]=='4') return JO_TP_ULONG;

    return 0;
}

static inline int jo_is_alphanum(unsigned char c) {
    return (c - '0' < 10) |
           ((c | 32) - 'a' < 26);
}

char jo_parse_uint_full(unsigned long *out, const char *buf, size_t len) {
  if(!len) return 0;
  
  int base = 10;
  size_t i = 0;

  if (len > 2 && buf[0] == '0') {
    switch (buf[1]) {
      case 'b': base = 2; i = 2; break;
      case 'o': base = 8; i = 2; break;
      case 'x': base = 16; i = 2; break;
    }
  }

  size_t num_end = i;
  while(num_end < len) {
    char c = buf[num_end];
    if (c == '_' || jo_is_alphanum(c)) continue;
    else break;
  }

  if(!jo_parse_uint(out, buf, len, base))
    return 0;

  char tp = jo_suffix_tp(&buf[num_end], len - num_end);

  return tp == 0? JO_TP_INFER : tp;
}

// void jo_asty(jo_token* tokens, jo_ast* list){

//   enum {_NONE, _KW, _ATOM, _BIN, _UN} state = _ATOM;
//   jo_token * seq[100];
//   int depth = 0;
  
//   int sp = 0;
//   jo_ast stack[100];

//   for(jo_token* t = tokens; ; t++){
//     if(t->type == 0) break;
//     if(t->type == JO_TOKEN_EMPTY) continue;
//     jo_ast* cur = &stack[sp];
    
//     switch (t->type) {
//       case JO_TOKEN_ID:
//       case JO_TOKEN_NUM:
//         cur->type = JO_AST_ID;
//         cur->atom.val = t->val;
//         cur->atom.len = t->len;
//         break;
//     }
//   }

//   for(jo_token* t = tokens; ; t++){
//     if(t->type == 0) break;
//     if(t->type == JO_TOKEN_EMPTY) continue;
//     if(t->depth != depth) continue;

//     switch (state) {
//       case _NONE:
//         if(t->type == JO_TOKEN_SYM) state = _UN;
//         else if(t->type == JO_TOKEN_KEYWORD) state = _KW;
//         else state = _ATOM;
//         break;
//       case _ATOM:
//         if(t->type == JO_TOKEN_SYM) state = _BIN;
//         break;
//       case _KW
//     }

//     if(t->type == JO_TOKEN_SYM && t->data == SYM_PLUS) {
//       t->type = JO_TOKEN_SYM;
//       t->data = JO_BIN_ADD;
//     }
//   }
// }

// enum {
//     JO_BIN_ADD = SYM_PLUS,
//     JO_BIN_SUB = SYM_MINUS,
//     JO_BIN_MUL = SYM_STAR,
//     JO_BIN_DIV = SYM_SLASH,
//     JO_BIN_MOD = SYM_MOD_EQ,
//     JO_BIN_XOR = SYM_CARET,

//     JO_BIN_LT = SYM_LT,
//     JO_BIN_GT = SYM_GT,
//     JO_BIN_LTE = SYM_LTE,
//     JO_BIN_GTE = SYM_GTE,


//     JO_BIN_EQ = SYM_EQ,
//     JO_BIN_EQEQ = SYM_EQEQ,
//     JO_BIN_EQEQEQ = SYM_REF_EQ,

//     JO_BIN_CONCAT = SYM_INC,
// };

/*

foo(x) if x 0
foo(x) if x + y < 0 z
bar(x) x{foo}

if x y else z
if x -y -z

ATOMS
A()
A[]
A{}
A..B
A:B
keyword A {}
keywordX A A {}

COMPOSITES
A B //pair


A(b) //func call

A B   //pair
*/

void jo_parse(jo_token* tokens) {

}

// clang-format on

// clang-format off


//
//=======================================
//

// clang-format on
#include <stdio.h>

void print_token(const struct jo_token *t) {
  if (!t->type || t->type == JO_TOKEN_EMPTY)
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
  const char *input = "foo(x int, y int) x + y + z";
  jo_cursor cursor = {};
  jo_token token = {};
  jo_token tokens[1000];
  jo_token *stack[1000];
  int i = 0;

  while (jo_tok(input, &cursor, &tokens[i++]))
    ;
  jo_tok_sym(tokens);
  jo_lex(tokens, stack);
  jo_iden(tokens);
  for (int j = 0; j < i; j++) {
    print_token(&tokens[j]);
  }
  printf("\n");
  return 0;
}

void jo_echo(const char *msg, size_t len) { printf("%.*s", (int)len, msg); }

void jo_stry_int(long i, char *b) { sprintf(b, "%ld", i); }

void jo_stry_uint(unsigned long i, char *b) { sprintf(b, "%lu", i); }

void jo_stry_float(double f, char *b) {
  sprintf(b, "%.6f", f); // tweak precision if needed
}

void jo_stry_bool(int v, char *b) { sprintf(b, "%s", v ? "true" : "false"); }