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
  TOKEN_INTERP, //$
  TOKEN_ESCAPE, //temporary
  TOKEN_STR1, // INTERPOLATABLE
  TOKEN_STR2, // NON INTERPOLATABLE
  TOKEN_SLUG,
  TOKEN_COMMENT1, // //
  TOKEN_COMMENT2, // /*
};
typedef enum TokenType TokenType;

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


static inline TokenType char_code(char c) {
    return is_whitespace(&c) ? TOKEN_SEP :
           is_alpha(&c)      ? TOKEN_ID  :
           is_digit(&c)      ? TOKEN_NUM :
                               TOKEN_SYM;
}


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
		int do_pop = 0;
		int do_normal = 0; // fallback
		int do_remove = 0; // kill this
		int do_escape = 0; // escape token
		int do_append = 0; // kill this and append to current
		int do_repeat = 0;

		switch (t->type) {

		case TOKEN_ID:
		case TOKEN_NUM:
			if (escape == '$') do_upgrade = t->type;
			else if(in_atom) do_append = 1;
			else if (!in_slug) do_push = t->type;
			else if (escape == '.' || escape == '_') do_upgrade = t->type;
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
				if (cur->type == TOKEN_ID) {
					int key = get_keyword(t->val);
					if (key) {
						cur->type = TOKEN_KEY;
						cur->data = key;
					}
				}
			}
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
    struct { const char *op; Node *a,*b; } bin;
    struct { enum Keyword kw; Node *subject; Node **body; int body_len; } flow;
    struct { enum Keyword kw; Node *payload; } control;
    struct { Node *what; Node **body; int body_len; } block;
    struct { Node *a,*b; } pair;
    struct { Node *name,*type,*init; } decl;
    struct { Node *name,*val; } assign;
  };
};




static inline int match(const Token* token, size_t tokens_len, int pos, enum TokenType type, const char* val, size_t name_len) {
	if (pos >= tokens_len)
		return 0;
	if (type != 0 && token[pos].type != type)
		return 0;
	if (val) {
		if (token[pos].len != name_len)
			return 0;
		if (strncmp(token[pos].val, val, name_len))
			return 0;
	}
	return 1;
}

#define match_val(pos, val) match(p->tokens, p->tokens_len, i, 0, val, sizeof(val) - 1)
#define match_type_val(pos, type, val) match(p->tokens, p->tokens_len, i, type, val, sizeof(val) - 1)
#define match_type(pos, type) match(p->tokens, p->tokens_len, i, type, NULL, 0)




struct Parser {
	Node* nodes;
	size_t nodes_len;
	size_t nodes_cap;

	const Token* tokens;
	size_t tokens_len;
};

int parse_id(Parser* p, int i, Node* out) {
	if(match_type(i, TOKEN_ID)) return i;
	out->t = AST_ID;
	out->id.name = p->tokens[i].val;
	out->id.len = p->tokens[i].len;

	int mut = match_type_val(++i, TOKEN_SYM, "`");
	
	if(mut) i++;
	return i;
}

int parse_op(Parser* p, int i, char** out) {
	
}

int parse_bin(Parser* p, int i, Node* out, Node* a, Node* b) {
	if(!match_type(i, TOKEN_SYM)) return 0;

	int ip = i;
	i = parse_id(p, i, out);
	if(i == ip) return 0;

	char* op = NULL;
	ip = i;
	i = parse_op(p, i, &op);
	if(i == ip) return 0;

	ip = i;
	i = parse_id(p, i, out);
	if(i == ip) return 0;

	out->t = AST_BIN;
	out->bin.op = p->tokens[i].val;
	out->bin.a = a;
	out->bin.b = b;
	return i+1;
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



int match_id(const Token* tokens, int pos, size_t tokens_len, Node* out) {
	if(!match_type(pos, TOKEN_ID)) return 0;

	int mut = match_type_val(pos+1, TOKEN_SYM, "`");

	out->t = AST_ID;
	out->id.name = tokens[pos].val;
	out->id.len = tokens[pos].len;
	out->id.mut = mut;
	return  1;
}

int match_lit(const Token* tokens, int pos, size_t tokens_len, Node* out) {
	if(!match_type(pos, TOKEN_NUM)) return 0;

	out->t = AST_LIT;
	out->lit.name = tokens[pos].val;
	out->lit.len = tokens[pos].len;
	return  1;
}

int match_node(const Token* tokens, int pos, size_t tokens_len, Node* out) {
	if(match_id(tokens, pos, tokens_len, out)) return 1;
	if(match_lit(tokens, pos, tokens_len, out)) return 1;
	//TODO
	return 0;
}



int astify(const Token* tokens, int pos, size_t tokens_len, Node * arena) {
	int arena_len = 0;

	const Token* last = NULL;

	printf("\nastify");
	while(pos < tokens_len) {
		const Token* t = &tokens[pos++];
		if(!t->type)
			continue;
		if(last && t->parent == last) //going down
			pos = astify(t, pos, tokens_len, arena);
		else if(pos >= tokens_len || last && last->parent != tokens[pos].parent) //going up
			break;

		printf(" %s", token_type_name(t->type));

		last = t;
	}
	printf("\n");

	return pos;
}
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

int main() {

	Tokenizer tok;
	// const char cline[] = "{//\n}";

	// int pos = skip_cline(cline, sizeof(cline) - 1);
	// printf("cline skipped %d at %s\n", pos, cline + pos);

	// return 0;


	const char src[] = "foo(){'hi$1 ${this is interpolation code () its fire}''you are donky;'}da";
	tokenizer_init(&tok, src, sizeof(src) - 1);

	static Token list[1000 * 1000];
	static Token* stack[200];

	size_t len = tokens(&tok, list, 1000 * 1000);
	lexize(list, len, stack);

	astify(list, 0, len, NULL);

	for (int i = 0; i < len; i++) {
		print_token(&list[i]);
	}
}
#endif