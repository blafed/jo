#include <stddef.h>
#include <stdint.h>
#include <string.h>

extern int printf(const char *fmt, ...);

// clang-format off
static inline int is_digit(const char *c) { return *c >= '0' && *c <= '9'; }
static inline int is_alpha(const char *c) { return (*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z'); }
static inline int is_alphanum(const char *c) { return is_digit(c) || is_alpha(c); }
static inline int is_whitespace(const char *c) { return *c == ' ' || *c == '\t' || *c == '\n'; }
static inline int is_control(const char *c) { return (*c < ' ' || *c == 0x7F) && !is_whitespace(c); }
static inline int is_newline(const char *c) { return *c == '\n'; }



enum TokenType : char {
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
  TOKEN_ESCAPE, //temporary
  TOKEN_STR1, // INTERPOLATABLE
  TOKEN_STR2, // NON INTERPOLATABLE
  TOKEN_SLUG,
  TOKEN_COMMENT1, // //
  TOKEN_COMMENT2, // /*
};
typedef enum TokenType TokenType;

static const unsigned char CHAR_CODE[256] = {
    [0 ... 255] = TOKEN_NONE,

    [' ']  = TOKEN_SEP,
    ['\t'] = TOKEN_SEP,
    ['\n'] = TOKEN_SEP,

    ['0' ... '9'] = TOKEN_NUM,
    ['a' ... 'z'] = TOKEN_ID,
    ['A' ... 'Z'] = TOKEN_ID,
};


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


static inline TokenType char_code(char c) {
    return is_whitespace(&c) ? TOKEN_SEP :
           is_alpha(&c)      ? TOKEN_ID  :
           is_digit(&c)      ? TOKEN_NUM :
                               TOKEN_SYM;
}

// static inline TokenType char_code(char c) {
//     return 
// }


typedef struct {
	const char *src;
	int pos, line, col, len;
} Tokenizer;

typedef struct Token {
	enum TokenType type;
	char data; //optional
	unsigned short len;

	int pos, line, col;
	
	const char *val;
	struct Token *parent;

} Token;




static inline void tokenizer_init(Tokenizer *t, const char *src, size_t len) {
	t->src = src;
	t->pos = 0;
	t->line = 1;
	t->col = 0;
	t->len = len;
}

static inline void tokenizer_advance(Tokenizer *tok) {

}

//the text SHOULD be null terminated
int token(Tokenizer *tok, Token *out) {
	int mpos = tok->pos, mcol = tok->col, mline = tok->line;
	int start = tok->pos;

	TokenType past = 0;

	if(tok->pos == tok->len) return 0;

	while(1) {
		char c = tok->src[tok->pos];
		TokenType code = CHAR_CODE[(unsigned char)c];

		char emit = 0;

		switch (code) {
			case TOKEN_ID:
			case TOKEN_NUM:
				if(past != code) emit = 1;
				break;
			case TOKEN_SEP:
				if(past != code || tok->src[tok->pos - 1] != c) emit = 1;
				break;
			case TOKEN_SYM: emit = 1; break;
			default: emit = 1; break;
		}
		if(emit && past){
			*out = (Token){.type = past, .val = tok->src + start, .len = tok->pos - start, .pos = mpos, .line = mline, .col = mcol}; 
			return 1;
		}
		tok->pos++;
		tok->col++;
		if(c == '\n') {
			tok->line++;
			tok->col = 0;
		}
		past = code;
	}

	return 0;
}

int token1(Tokenizer *tok, Token *out) {
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
      		*out = (Token){.type = prev, .val = tok->src + start, .len = tok->pos - start, .pos = mpos, .line = mline, .col = mcol};
			return 1;
		}

		prev = code;
	}

	if (prev) {
    	*out = (Token){.type = prev, .val = tok->src + start, .len = tok->pos - start, .pos = mpos, .line = mline, .col = mcol};
		return 1;
	}

	*out = (Token){};
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

enum Keyword { KEY_NONE, KEY_IF, KEY_ELSE, KEY_WHILE, KEY_DO, KEY_RETURN, KEY_BREAK, KEY_CONTINUE };

static inline enum Keyword get_keyword(const char *c, size_t len) {
    switch (len) {
    case 2:
        if (c[0]=='i' && c[1]=='f') return KEY_IF;
        if (c[0]=='d' && c[1]=='o') return KEY_DO;
        break;

    case 4:
        if (!memcmp(c, "else", 4)) return KEY_ELSE;
        break;

    case 5:
        if (!memcmp(c, "while", 5)) return KEY_WHILE;
        if (!memcmp(c, "break", 5)) return KEY_BREAK;
        break;

    case 6:
        if (!memcmp(c, "return", 6)) return KEY_RETURN;
        break;

    case 8:
        if (!memcmp(c, "continue", 8)) return KEY_CONTINUE;
        break;
    }
    return KEY_NONE;
}

static inline void sum_token(Token *a, Token *b) {
	a->len += b->len;
	*b = (Token){};
}

void lexize(Token *tokens, size_t tokens_len, Token* stack[]) {
	// Token *stack[200];
	static Token root = {};
	size_t stack_len = 1;
	stack[0] = &root;

	int pos = 0;
	while (pos < tokens_len) {
		Token *t = &tokens[pos++];
		Token *cur = stack[stack_len - 1];

		const int in_string = cur->type == TOKEN_STR1 || cur->type == TOKEN_STR2;
		const int in_comment = cur->type == TOKEN_COMMENT1 || cur->type == TOKEN_COMMENT2;
		const int in_slug = in_string || in_comment;
		const int in_atom = cur->type == TOKEN_ID || cur->type == TOKEN_NUM;
		const char escape = cur->type == TOKEN_ESCAPE ? cur->val[0] : 0;

		// simple parenting
		t->parent = cur;

		int do_push = TOKEN_NONE;
		int do_upgrade = TOKEN_NONE; // upgrade current's type
		int do_pop = 0; //1 -> pop and consume this. 2 -> pop and keep this
		int do_normal = 0; // fallback
		int do_remove = 0; // kill this
		int do_escape = 0; // escape token
		int do_append = 0; // kill this and append to current

		switch (t->type) {

		case TOKEN_ID:
		case TOKEN_NUM:
			if (escape == '$') do_upgrade = t->type;
			else if(in_atom) do_append = 1;
			else if (escape) do_pop = 2;
			else if (escape == '.' || escape == '_') do_upgrade = t->type;
			else if (!in_slug) do_push = t->type;
			else do_normal = 1;
			break;

		case TOKEN_SYM:
			if (in_atom && (t->val[0] != '_' && t->val[0] != '.')) {
				do_pop = 2;
				break;
			}

			if(escape == '$' && t->val[0] != '{') {
				do_pop = 2;
				break;
			}

			switch (t->val[0]) {
			case '.':
				if (in_atom) do_append = 1;
				else if (!in_slug) do_escape = 1;
				else do_normal = 1;
				break;
			case '_':
				if(in_atom) do_append = 1;
				else if(!in_slug) do_push = TOKEN_ID;
				else do_normal = 1;
				break;
			case '{':
				if (escape == '$') do_upgrade = TOKEN_BLOCK;
				else if (!in_slug) do_push = TOKEN_BLOCK;
				else do_normal = 1;
				break;
			case '(':
				if (!in_slug) do_push = TOKEN_PAREN;
				else do_normal = 1;
				break;
			case '[':
				if (!in_slug) do_push = TOKEN_BOX;
				else do_normal = 1;
				break;
			case '}':
				if (cur->type == TOKEN_BLOCK) do_pop = 1;
				else do_normal = 1;
				break;
			case ']':
				if (cur->type == TOKEN_BOX) do_pop = 1;
				else do_normal = 1;
				break;
			case ')':
				if (cur->type == TOKEN_PAREN) do_pop = 1;
				else do_normal = 1;
				break;
			case '$':
				if (!in_comment || cur->type == TOKEN_STR1) do_escape = 1;
				else do_normal = 1;
				// do_push = TOKEN_INTERP; // interp anything
				break;
			case '\'':
				if (in_string) do_pop = 1;
				else if (escape == '$') do_upgrade = TOKEN_STR2; //$''
				else do_push = TOKEN_STR1; // begin string
				// else do_normal =1;
				break;
			case '/':
				if (escape == '/') do_upgrade = TOKEN_COMMENT1;
				else if (escape == '*' && cur->type == TOKEN_COMMENT2) do_pop = 1;
				else do_push = TOKEN_ESCAPE;
				break;

			case '*':
				if (escape == '/') do_upgrade = TOKEN_COMMENT2;
				else if (cur->type == TOKEN_COMMENT2) do_push = TOKEN_ESCAPE;
				break;

				//TODO use seq instead
			case '+':
				if(escape == '+') do_append = 1;
				else if(!in_slug) do_escape = 1;
				else do_normal = 1;
				break;
			case '-':
				if(escape == '-') do_append = 1;
				else if(!in_slug) do_escape = 1;
				else do_normal = 1;
				break;
			case ':':
				if(escape == ':') do_append = 1;
				else if(!in_slug) do_escape = 1;
				else do_normal = 1;
				break;
			case '=':
				if(escape == ':' || escape == '=' || escape == '+' || escape == '-' || escape == '*' || escape == '/' || escape == '&' || escape == '|' || escape == '^') do_append = 1;
				else if(!in_slug) do_escape = 1;
				else do_normal = 1;
				break;
			case '&':
				if(escape == '&') do_append = 1;
				else if(!in_slug) do_escape = 1;
				else do_normal = 1;
				break;
			case '|':
				if(escape == '|') do_append = 1;
				else if(!in_slug) do_escape = 1;
				else do_normal = 1;
				break;
			case '?':
				if(escape == '?') do_append = 1;
				else if(!in_slug) do_escape = 1;
				else do_normal = 1;
				break;
			}

			break;

		case TOKEN_SEP:
			if (cur->type == TOKEN_ESCAPE || in_atom) do_pop = 1;
			else if (cur->type == TOKEN_COMMENT1 || is_newline(t->val)) do_pop = 1;
			else do_normal = 1;
			break;
		}

		if (do_push) {
			stack[stack_len++] = t;
			t->type = do_push;
		}
		else if (do_upgrade) {
			cur->len += t->len;
			cur->type = do_upgrade;
			t->type = TOKEN_NONE;
			t->len = 0;
		}
		else if (do_pop) {
			stack_len--;
			if (do_pop == 2) pos--; // keep
			else {	   				// consume
				t->type = TOKEN_NONE;
			}

			if (cur->type == TOKEN_ID) {
				int key = get_keyword(cur->val, cur->len);
				if (key) {
					cur->type = TOKEN_KEY;
					cur->data = key;
				}
			}
			else if(cur->type == TOKEN_ESCAPE)
				cur->type = TOKEN_SYM;
		}
		else if (do_normal) {
			if (in_slug)
				t->type = TOKEN_SLUG;
		}
		else if (do_remove) {
			t->type = TOKEN_NONE;
			t->len = 0;
		}
		else if (do_escape) {
			stack[stack_len++] = t;
			t->type = TOKEN_ESCAPE;
		}
		else if (do_append) {
			cur->len += t->len;
			t->type = TOKEN_NONE;
			t->len = 0;
		}
	}
}

int skip_cline(const char* start, size_t len) {
    if (len == 0 || start[0] != '{')
        return -1;

    char stack[64];
    char sp = 1;
    int i = 1;
    stack[0] = '{'; //root

    while (i < len) {
        char s = stack[sp - 1];
        char c = start[i];
        char n = (i + 1 < len) ? start[i + 1] : 0;

        char do_push = 0;
        unsigned char do_pop  = 0;

        /* escape inside string / char */
        if ((s == '"' || s == '\'') && c == '\\') {
            i += 2;
            continue;
        }

        switch (c) {

        /* ---- closing ---- */
        case '}':
            if (s == '{') do_pop = 1;
            break;

        case '"':
            if (s == '"') do_pop = 1;
            else if (s == '{') do_push = '"';
            break;

        case '\'':
            if (s == '\'') do_pop = 1;
            else if (s == '{') do_push = '\'';
            break;

        case '\n':
            if (s == '/') do_pop = 1;
            if (s == '#' && start[i - 1] != '\\') do_pop = 1;
            break;

        /* ---- opening ---- */
        case '{':
            if (s == '{') do_push = '{';
            break;

        case '/':
            if (s == '{' && n == '/') do_push = '/', i++;
            else if (s == '{' && n == '*') do_push = '*', i++;
            break;

        case '*':
            if (s == '*' && n == '/') do_pop = 1, i++;
            break;

        case '#':
            if (s == '{') do_push = '#';
            break;
        }

        i++;

        if (do_pop) {
            sp--;
            if (sp == 0)
                return i - 1;
        } else if (do_push) {
            stack[sp++] = do_push;
        }
    }

    return -1;
}

#include <stdlib.h>

typedef struct Parser Parser;
typedef struct Node Node;

// clang-format off
typedef enum { AST_NONE, AST_ID, AST_LIT, AST_WRAP, AST_FUNC, AST_CALL, AST_NAMESPACE, AST_BIN, AST_FLOW, AST_CONTROL, AST_BLOCK, AST_PAIR, AST_DECL, AST_ASSIGN } AstType;
typedef enum { MOD_NONE, MOD_REF, MOD_REX, MOD_NEW, MOD_OPT, MOD_MOV, MOD_NOT, MOD_NOX, MOD_NEG, MOD_POS } AstMod;
typedef enum : u_int8_t {OP_NONE, OP_SUM, OP_SUB, OP_MUL, OP_DIV, OP_CAT, OP_INC, OP_DEC, OP_AND, OP_OR, OP_XOR, OP_NOT, OP_EQ, OP_NEQ, OP_LT, OP_LE, OP_GT, OP_GE, OP_SHL, OP_SHR, OP_LSH, OP_RSH, OP_ANDAND, OP_OROR} OpType;

struct Node {
  AstType t;
  const Token* token; //nullable

  union {
    struct { const char *name; unsigned short len; unsigned char mut; } id;
	struct {const char* name; unsigned short len; } lit;
    struct { Node *x,*y; AstMod mod; unsigned pre:1; } wrap;
    struct { Node **args; int args_len; Node *ret_type; Node **body; int body_len; } func;
    struct { Node *func; Node **args; int args_len; } call;
    struct { const char *name; Node **body; int body_len; } ns;
    struct { OpType op; Node *a,*b; } bin;
    struct { enum Keyword kw; Node *subject; Node **body; int body_len; } flow;
    struct { enum Keyword kw; Node *payload; } control;
    struct { Node *what; Node **body; int body_len; } block;
    struct { Node *a,*b; } pair;
    struct { Node *name,*type,*init; } decl;
    struct { Node *name,*val; } assign;
  };
};




static inline int match(const Token* token, size_t tokens_len, int* pos, enum TokenType type, const char* val, size_t val_len) {
	while(*pos < tokens_len && token[*pos].type == TOKEN_NONE)
		pos++;
	if (*pos >= tokens_len)
		return 0;
	if (type != 0 && token[*pos].type != type)
		return 0;
	if (val) {
		if (token[*pos].len != val_len)
			return 0;
		if (strncmp(token[*pos].val, val, val_len))
			return 0;
	}
	return 1;
}

#define match_val(pos, val) match(p->tokens, p->tokens_len, &pos, 0, val, sizeof(val) - 1)
#define match_type_val(pos, type, val) match(p->tokens, p->tokens_len, &pos, type, val, sizeof(val) - 1)
#define match_type(pos, type) match(p->tokens, p->tokens_len, &pos, type, NULL, 0)




struct Parser {
	Node* nodes;
	size_t nodes_len;
	size_t nodes_cap;

	const Token* tokens;
	size_t tokens_len;

	int pos;
	int rpos;
};

void init_parser(Parser* p, const Token* tokens, size_t tokens_len) {
	p->tokens = tokens;
	p->tokens_len = tokens_len;
} 

OpType parse_opcode(const char* c, size_t len) {
    switch (len) {
    case 1:
        switch (c[0]) {
        case '+': return OP_SUM;
        case '-': return OP_SUB;
        case '*': return OP_MUL;
        case '/': return OP_DIV;
        case '!': return OP_NOT;
        case '<': return OP_LT;
        case '>': return OP_GT;
        case '&': return OP_AND;
        case '|': return OP_OR;
        case '^': return OP_XOR;
        default:  return OP_NONE;
        }

    case 2:
        if (c[0] == '+' && c[1] == '+') return OP_INC;
        if (c[0] == '-' && c[1] == '-') return OP_DEC;
        if (c[0] == '=' && c[1] == '=') return OP_EQ;
        if (c[0] == '!' && c[1] == '=') return OP_NEQ;
        if (c[0] == '<' && c[1] == '=') return OP_LE;
        if (c[0] == '>' && c[1] == '=') return OP_GE;
        if (c[0] == '&' && c[1] == '&') return OP_ANDAND;
        if (c[0] == '|' && c[1] == '|') return OP_OROR;
        if (c[0] == '<' && c[1] == '<') return OP_SHL;
        if (c[0] == '>' && c[1] == '>') return OP_SHR;
        return OP_NONE;

    case 3:
        if (c[0] == '<' && c[1] == '<' && c[2] == '<') return OP_LSH;
        if (c[0] == '>' && c[1] == '>' && c[2] == '>') return OP_RSH;
        return OP_NONE;

    default:
        return OP_NONE;
    }
}

static const char* op_str(OpType op, int* len) {
    switch (op) {
    case OP_SUM: *len = 1; return "+";
    case OP_SUB: *len = 1; return "-";
    case OP_MUL: *len = 1; return "*";
    case OP_DIV: *len = 1; return "/";
	case OP_NOT: *len = 1; return "!";
	case OP_AND: *len = 1; return "&";
	case OP_OR:  *len = 1; return "|";
    case OP_EQ:  *len = 2; return "==";
    case OP_NEQ: *len = 2; return "!=";
    case OP_ANDAND: *len = 2; return "&&";
    case OP_OROR:  *len = 2; return "||";
	case OP_INC:  *len = 2; return "++";
	case OP_DEC:  *len = 2; return "--";
	case OP_SHL:  *len = 2; return "<<";
	case OP_SHR:  *len = 2; return ">>";
	case OP_LSH:  *len = 3; return "<<<";
	 case OP_RSH:  *len = 3; return ">>>";
    default: *len = 1; return "?";
    }
}

static int op_prec(OpType op) {
    switch (op) {
    case OP_OROR:   return 1;
    case OP_ANDAND: return 2;
    case OP_OR:     return 3;
    case OP_XOR:    return 4;
    case OP_AND:    return 5;
    case OP_EQ:
    case OP_NEQ:    return 6;
    case OP_LT:
    case OP_LE:
    case OP_GT:
    case OP_GE:     return 7;
    case OP_SHL:
    case OP_SHR:
    case OP_LSH:
    case OP_RSH:    return 8;
    case OP_SUM:
    case OP_SUB:    return 9;
    case OP_MUL:
    case OP_DIV:    return 10;
    default:        return 0;
    }
}



Node* node_alloc() {return malloc(sizeof(Node));}
void parse(const Token* tokens, size_t tokens_len, Node* out, Node* stack[], Node* (*alloc)()) {
	int sp = 0;
	int pos = 0;


	
	for(;;pos++) {
		if(pos >= tokens_len) break;

		int do_push = 0;

		const Token* t = &tokens[pos];

		switch (t->type) {
			case TOKEN_NONE: continue;
			case TOKEN_ID: do_push = 1; break;
			case TOKEN_SYM: do_push = 1; break;
		}


		if(do_push) {
			stack[sp++] = alloc();
		}
	}
	printf("sp is %d\n", sp);
}


void parser_begin(Parser* p) {
	p->rpos = p->pos;
}
void parser_end(Parser* p) {
	p->pos = p->rpos;
}
int parser_result(Parser* p) {return p->pos - p->rpos;}
const Token* parser_next(Parser* p){
	while(p->pos < p->tokens_len) {
		if(p->tokens[p->pos].type == TOKEN_NONE) p->pos++;
		else return &p->tokens[p->pos];
	}
	return NULL;
}


void parse_id(Parser* p, int i, Node* out) {
	parser_begin(p);

	const Token* t = parser_next(p);
	if(t && t->type == TOKEN_ID) {
		out->t = AST_ID;
		out->id.name = t->val;
		out->id.len = t->len;
		p->pos += t->len;
	}

	parser_end(p);
}


int parse_ids(Parser* p, int i, Node* out) {
	if(!match_type(i, TOKEN_ID)) return i;
	out->t = AST_ID;
	out->id.name = p->tokens[i].val;
	out->id.len = p->tokens[i].len;

	i++;
	int mut = match_type_val(i, TOKEN_SYM, "`");
	
	if(mut) i++;
	return i;
}


int parse_op(Parser* p, int i, OpType* out) {
	if(!match_type(i, TOKEN_SYM)) return 0;
	
	OpType res = parse_opcode(p->tokens[i].val, p->tokens[i].len);
	if(res == OP_NONE) return 0;
	else {
		*out = res;
		return i + p->tokens[i].len;
	}
}

// int parse_bin(Parser* p, int i, Node* out, Node* a, Node* b) {
// 	int ip = i;
// 	i = parse_id(p, i, a);
// 	if(i == ip) return 0;

// 	OpType op;
// 	ip = i;
// 	i = parse_op(p, i, &op);
// 	if(i == ip) return 0;

// 	ip = i;
// 	i = parse_id(p, i, b);
// 	if(i == ip) return 0;

// 	out->t = AST_BIN;
// 	out->bin.op = op;
// 	out->bin.a = a;
// 	out->bin.b = b;
// 	return i;
// }


static int buf_put(char* buf, int at, const char* s, int len) {
    for (int i = 0; i < len; i++) buf[at++] = s[i];
	buf[at] = 0;
    return at;
}

static int buf_putc(char* buf, int at, char c) {
    buf[at++] = c;
	buf[at] = 0;
    return at;
}


static inline int needs_paren(Node* parent, Node* child) {
    if (!child) return 0;
    if (child->t != AST_BIN) return 0;
    return op_prec(child->bin.op) < op_prec(parent->bin.op);
}

int print_node(Node* node, char* buf) {
    int at = 0;

    if (!node) {
        buf[0] = '?';
        buf[1] = 0;
        return 1;
    }

    switch (node->t) {

    case AST_ID:
        return buf_put(buf, 0, node->id.name, node->id.len);

    case AST_BIN: {
        // left
        if (needs_paren(node, node->bin.a))
            at = buf_putc(buf, at, '(');

        at += print_node(node->bin.a, buf + at);

        if (needs_paren(node, node->bin.a))
            at = buf_putc(buf, at, ')');

        at = buf_putc(buf, at, ' ');

        // op
        int oplen;
        const char* op = op_str(node->bin.op, &oplen);
        at = buf_put(buf, at, op, oplen);

        at = buf_putc(buf, at, ' ');

        // right
        if (needs_paren(node, node->bin.b))
            at = buf_putc(buf, at, '(');

        at += print_node(node->bin.b, buf + at);

        if (needs_paren(node, node->bin.b))
            at = buf_putc(buf, at, ')');

        return at;
    }

    default:
        buf[0] = '?';
        buf[1] = 0;
        return 1;
    }
}



static Node * parser_alloc(Parser*p) {
	if(p->nodes_len == p->nodes_cap) {
		p->nodes_cap *= 2;
		Node* tmp = realloc(p->nodes, p->nodes_cap * sizeof(Node));

		if(tmp == p->nodes)
			p ->nodes = tmp;
	}
}
// clang-format on



// int match_id(const Token* tokens, int pos, size_t tokens_len, Node* out) {
// 	if(!match_type(pos, TOKEN_ID)) return 0;

// 	int mut = match_type_val(pos+1, TOKEN_SYM, "`");

// 	out->t = AST_ID;
// 	out->id.name = tokens[pos].val;
// 	out->id.len = tokens[pos].len;
// 	out->id.mut = mut;
// 	return  1;
// }

// int match_lit(const Token* tokens, int pos, size_t tokens_len, Node* out) {
// 	if(!match_type(pos, TOKEN_NUM)) return 0;

// 	out->t = AST_LIT;
// 	out->lit.name = tokens[pos].val;
// 	out->lit.len = tokens[pos].len;
// 	return  1;
// }

// int match_node(const Token* tokens, int pos, size_t tokens_len, Node* out) {
// 	if(match_id(tokens, pos, tokens_len, out)) return 1;
// 	if(match_lit(tokens, pos, tokens_len, out)) return 1;
// 	//TODO
// 	return 0;
// }



// int astify(const Token* tokens, int pos, size_t tokens_len, Node * arena) {
// 	int arena_len = 0;

// 	const Token* last = NULL;

// 	printf("\nastify");
// 	while(pos < tokens_len) {
// 		const Token* t = &tokens[pos++];
// 		if(!t->type)
// 			continue;
// 		if(last && t->parent == last) //going down
// 			pos = astify(t, pos, tokens_len, arena);
// 		else if(pos >= tokens_len || last && last->parent != tokens[pos].parent) //going up
// 			break;

// 		printf(" %s", token_type_name(t->type));

// 		last = t;
// 	}
// 	printf("\n");

// 	return pos;
// }
// clang-format off

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


void print_token(const Token *t) {
	if (!t->type)
		return;
	int depth = 0;
	Token *par = t->parent;
	while (par) {
		fputs(" |", stdout);
		par = par->parent;
	}
	printf("%-3s @%d:%d \"", token_type_name(t->type), t->line, t->col);

	fwrite(t->val, 1, t->len, stdout);
	puts("\"");
}

static long _read_file(const char *path, uint8_t **buffer) {
	FILE *file = fopen(path, "rb");
	if (!file)
		goto FAIL;

	fseek(file, 0, SEEK_END);
	long size = ftell(file);
	rewind(file);

	if (size < 0)
		goto FAIL;

	*buffer = (uint8_t *)malloc(size + 1);
	if (!*buffer)
		goto FAIL;
	(*buffer)[size] = 0;

	fread(*buffer, 1, size, file);

	fclose(file);
	return size;

FAIL:

	*buffer = NULL;
	if (file)
		fclose(file);
	return 0;
}

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <stdint.h>

int stress_test(void) {
    enum { SRC_CAP = 200 * 1024 * 1024, MAX_TOK = 50 * 1000 * 1000 };

    static const char ids[]  = "abcdefghijklmnopqrstuvwxyz";
    static const char syms[] = ".{}()[]+-=*/&|?:$'";

    // ---------- alloc ----------
    char *src = (char*)malloc(SRC_CAP);
    Token *tokens = (Token*)malloc(sizeof(Token) * MAX_TOK);
    if (!src || !tokens) return 1;

    // ---------- random source ----------
    uint32_t r = 123456789;
    size_t i = 0;

    #define RNG() (r = r * 1664525u + 1013904223u)

    while (i + 4 < SRC_CAP) {
        RNG();
        int k = r % 10;

        if (k < 3) {                    // id
            int n = 1 + (RNG() % 12);
            while (n-- && i + 1 < SRC_CAP)
                src[i++] = ids[RNG() % (sizeof(ids) - 1)];
        }
        else if (k < 5) {               // number
            int n = 1 + (RNG() % 10);
            while (n-- && i + 1 < SRC_CAP)
                src[i++] = '0' + (RNG() % 10);
        }
        else if (k < 6) {               // whitespace
            src[i++] = ' ';
            if (RNG() & 1) src[i++] = '\n';
        }
        else if (k < 8) {               // symbol
            src[i++] = syms[RNG() % (sizeof(syms) - 1)];
        }
        else {                          // string
            src[i++] = '\'';
            int n = RNG() % 8;
            while (n-- && i + 2 < SRC_CAP)
                src[i++] = ids[RNG() % (sizeof(ids) - 1)];
            src[i++] = '\'';
        }
    }

    src[i] = 0; // NULL TERMINATE
    size_t len = i;

    // ---------- tokenize ----------
    Tokenizer tok = {
        .src = src,
        .pos = 0,
        .len = len,
        .line = 1,
        .col = 0
    };

    clock_t t0 = clock();

    size_t count = 0;
    while (count < MAX_TOK && token(&tok, &tokens[count]))
        count++;

    clock_t t1 = clock();

    // ---------- report ----------
    double sec = (double)(t1 - t0) / CLOCKS_PER_SEC;

    printf("bytes   : %zu\n", len);
    printf("tokens  : %zu\n", count);
    printf("time    : %.3f sec\n", sec);
    printf("through : %.2f MB/s\n",
           (len / (1024.0 * 1024.0)) / sec);

    free(src);
    free(tokens);
    return 0;
}


int main() {
// stress_test();
	// return 0;
	Tokenizer tok;
	// const char cline[] = "{//\n}";

	// int pos = skip_cline(cline, sizeof(cline) - 1);
	// printf("cline skipped %d at %s\n", pos, cline + pos);

	// return 0;
	static Node nodes[1000];

	uint8_t* fbuf = NULL;
	long flen = _read_file("ex/index.jon", &fbuf);
	printf("fuck %ld\n", flen);
	

	
	
	// const char src[] = "if while x{0}+c";
	// tokenizer_init(&tok, src, sizeof(src) - 1);

	
	fbuf = "hello    world;;sads1312";
	flen = strlen(fbuf);
	tokenizer_init(&tok, fbuf, flen);
	
	static Token list[1000 * 1000];
	static Token* stack[200];
	
	size_t len = tokens(&tok, list, 1000 * 1000);
	// lexize(list, len, stack);
	
	
	static	Node* node_stack[1000];

	Node parsed;
	// parse(list, len, &parsed, node_stack);
	

	// Parser p;
	// init_parser(&p, list, len);
	// int parsed = parse_bin(&p, 0, &nodes[0], &nodes[1], &nodes[2]);

	// char buf[1000];

	// print_node(&nodes[0], buf);

	// if(parsed)
	// printf("%s\n", buf);

	// astify(list, 0, len, NULL);
nodes[0] = (Node){.t=AST_BIN,.bin={OP_MUL,&nodes[1],&nodes[2]}};
nodes[1] = (Node){.t=AST_ID,.id={"a",1}};
nodes[2] = (Node){.t=AST_BIN,.bin={OP_SUM,&nodes[3],&nodes[4]}};
nodes[3] = (Node){.t=AST_ID,.id={"b",1}};
nodes[4] = (Node){.t=AST_ID,.id={"c",1}};

	char buf[1000];
	print_node(&nodes[0], buf);
	printf("%s\n", buf);


	for (int i = 0; i < len; i++) {
		print_token(&list[i]);
	}
}
#endif