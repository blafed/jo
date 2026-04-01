#include <stddef.h>
#include <stdint.h>
#include <string.h>

extern int printf(const char *fmt, ...); //TODO remove this

enum TokenType : char {
  TOKEN_NONE,
  TOKEN_ID,  // sequence of alphabet (multople)
  TOKEN_NUM, // sequence of digits (multiple)
  TOKEN_SYM, // any symbol single only (single)
  TOKEN_SEP, // sequence of whitespace of same type, flattened (single)

  //phase2
  TOKEN_INTERP, //begins with $
  TOKEN_ROOT,
  TOKEN_KEY,
  TOKEN_GROUP, //{} [] () <>
  TOKEN_BLOCK, //{
  TOKEN_BOX, //[
  TOKEN_PAREN, //(
  TOKEN_ESCAPE, //temporary
  TOKEN_STR1, // '
  TOKEN_STR2, // "
  TOKEN_SLUG,
  TOKEN_COMMENT1, // //
  TOKEN_COMMENT2, // /*
};
typedef enum TokenType TokenType;
static const unsigned char CHAR_TYPE[256] = {
    #define SEP(x) [(unsigned char)(x)] = TOKEN_SEP
    #define NUM(x) [(unsigned char)(x)] = TOKEN_NUM
    #define ID(x)  [(unsigned char)(x)] = TOKEN_ID
    #define SYM(x) [(unsigned char)(x)] = TOKEN_SYM

    SEP(' '), SEP('\t'), SEP('\n'), SEP('\r'), SEP('\f'), SEP('\v'),

    NUM('0'), NUM('1'), NUM('2'), NUM('3'), NUM('4'),
    NUM('5'), NUM('6'), NUM('7'), NUM('8'), NUM('9'),

    ID('a'), ID('b'), ID('c'), ID('d'), ID('e'),
    ID('f'), ID('g'), ID('h'), ID('i'), ID('j'),
    ID('k'), ID('l'), ID('m'), ID('n'), ID('o'),
    ID('p'), ID('q'), ID('r'), ID('s'), ID('t'),
    ID('u'), ID('v'), ID('w'), ID('x'), ID('y'), ID('z'),

    ID('A'), ID('B'), ID('C'), ID('D'), ID('E'),
    ID('F'), ID('G'), ID('H'), ID('I'), ID('J'),
    ID('K'), ID('L'), ID('M'), ID('N'), ID('O'),
    ID('P'), ID('Q'), ID('R'), ID('S'), ID('T'),
    ID('U'), ID('V'), ID('W'), ID('X'), ID('Y'), ID('Z'),

    SYM('!'), SYM('"'), SYM('#'), SYM('$'), SYM('%'),
    SYM('&'), SYM('\''), SYM('('), SYM(')'), SYM('*'),
    SYM('+'), SYM(','), SYM('-'), SYM('.'), SYM('/'),

    SYM(':'), SYM(';'), SYM('<'), SYM('='), SYM('>'),
    SYM('?'), SYM('@'),

    SYM('['), SYM('\\'), SYM(']'), SYM('^'),
    SYM('_'), SYM('`'),

    SYM('{'), SYM('|'), SYM('}'), SYM('~'),

    #undef SEP
    #undef NUM
    #undef ID
    #undef SYM
};

static const char CHAR_PAIR[] = {
	['{'] = '}', ['('] = ')', ['<'] = '>', ['['] = ']',
	['}'] = '{', [')'] = '(', ['>'] = '<', [']'] = '[',
};

// clang-format off

static const char *token_type_name(enum TokenType t) {
  switch (t) {
  case TOKEN_NONE:     return "NONE";
  case TOKEN_ID:       return "ID";
  case TOKEN_NUM:      return "NUM";
  case TOKEN_SYM:      return "SYM";
  case TOKEN_SEP:      return "SEP";

  case TOKEN_GROUP:    return "GROUP";
  case TOKEN_INTERP:   return "INTERP";
  case TOKEN_ROOT:     return "ROOT";
  case TOKEN_KEY:      return "KEY";
  case TOKEN_BLOCK:    return "BLOCK";
  case TOKEN_BOX:      return "BOX";
  case TOKEN_PAREN:    return "PAREN";
  case TOKEN_ESCAPE:   return "ESC";
  case TOKEN_STR1:     return "STR";
  case TOKEN_STR2:     return "STR2";
  case TOKEN_SLUG:     return "SLUG";
  case TOKEN_COMMENT1: return "//";
  case TOKEN_COMMENT2: return "/*";

  default:             return "???";
  }
}
// clang-format on




typedef struct { const char *src; int pos, line, col, len; } Tokenizer;
typedef struct Token { 
	enum TokenType type;
	char data;
	unsigned short len;
	int pos, line, col;
	const char *val;
	struct Token *parent;
} Token;
void print_token(const Token *t);


static inline void tokenizer_init(Tokenizer *t, const char *src, size_t len) {
	t->src = src;
	t->pos = 0;
	t->line = 0;
	t->col = 0;
	t->len = len;
}

enum Keyword : unsigned char{ KEY_NONE, KEY_IF, KEY_ELSE, KEY_WHILE, KEY_DO, KEY_RETURN, KEY_BREAK, KEY_CONTINUE };
enum Symbol : unsigned char {
	SYM_NONE        = 0,

	SYM_EXCL        = '!',
	SYM_DQUOTE      = '"',
	SYM_HASH        = '#',
	SYM_DOLLAR      = '$',
	SYM_PERCENT     = '%',

	SYM_AMP         = '&',
	SYM_SQUOTE      = '\'',
	SYM_LPAREN      = '(',
	SYM_RPAREN      = ')',
	SYM_STAR        = '*',

	SYM_PLUS        = '+',
	SYM_COMMA       = ',',
	SYM_MINUS       = '-',
	SYM_DOT         = '.',
	SYM_SLASH       = '/',

	SYM_COLON       = ':',
	SYM_SEMI        = ';',
	SYM_LT          = '<',
	SYM_EQ          = '=',
	SYM_GT          = '>',

	SYM_QUESTION    = '?',
	SYM_AT          = '@',

	SYM_LBRACKET    = '[',
	SYM_BACKSLASH   = '\\',
	SYM_RBRACKET    = ']',
	SYM_CARET       = '^',
	SYM_UNDERSCORE  = '_',
	SYM_BACKTICK    = '`',

	SYM_LBRACE      = '{',
	SYM_PIPE        = '|',
	SYM_RBRACE      = '}',
	SYM_TILDE       = '~',

	SYMMANY = 128,


	SYM_DECL,       // :=
	SYM_NULLER,       // ??
	SYM_INC,        // ++
    SYM_DEC,        // --
    SYM_SHL,        // <<
    SYM_SHR,        // >>
    SYM_LOR,        // ||
    SYM_LAND,       // &&
    SYM_DOTDOT,     // ..
    SYM_ADD_EQ,     // +=
    SYM_SUB_EQ,     // -=
    SYM_MUL_EQ,     // *=
    SYM_DIV_EQ,     // /=
    SYM_MOD_EQ,     // %=
    SYM_XOR_EQ,     // ^=
    SYM_AND_EQ,     // &=
    SYM_OR_EQ,      // |=
    SYM_EQEQ,       // ==
    SYM_NEQ,        // !=
    SYM_LTE,        // <=
    SYM_GTE,        // >=
    SYM_ARROW,      // ->
    SYM_FATARROW,   // =>
    SYM_SCOPE,      // ::
	SYM_COMMENT1,   // //
	SYM_COMMENT2_BEGIN, 	// /*
	SYM_COMMENT2_END,   	// */

	SYM_REF_EQ,     // &==
	SYM_SHL_EQ,     // <<=
    SYM_SHR_EQ,     // >>=
	SYM_TRIPLE_DQUOTE,  // """
    SYM_TRIPLE_SQUOTE,  // '''
	SYM_TODO         // ???

};



static inline enum Symbol symbol(const char* c, size_t len) {
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
            case ':': if (c[1] == '=') return SYM_DECL; break;
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
                      break;
            case '!': if (c[1] == '=') return SYM_NEQ; break;
        }
        break;

    case 3:
        switch (c[0]) {
			case '&': if (c[1] == '=' && c[2] == '=') return SYM_REF_EQ; break;
            case '<': if (c[1] == '<' && c[2] == '=') return SYM_SHL_EQ; break;
            case '>': if (c[1] == '>' && c[2] == '=') return SYM_SHR_EQ; break;
            case ':': if (c[1] == ':' && c[2] == ':') return SYM_SCOPE; break;
            case '"': if (c[1] == '"' && c[2] == '"') return SYM_TRIPLE_DQUOTE; break;
            case '\'': if (c[1] == '\'' && c[2] == '\'') return SYM_TRIPLE_SQUOTE; break;
			case '?': if (c[1] == '?' && c[2] == '?') return SYM_TODO; break;
        }
        break;
    }

	return 0;
}


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

//the text SHOULD be null terminated
int token(Tokenizer *tok, Token *out) {
	out->pos = tok->pos; out->line = tok->line; out->col = tok->col;
	int start = tok->pos;

	TokenType past = 0;

	while(1) {
		char c = tok->src[tok->pos];
		TokenType code = CHAR_TYPE[(unsigned char)c];

		char emit = 0;

		switch (code) {
			case TOKEN_ID:
			case TOKEN_NUM:
				if(past != code) emit = 1;
				break;
			case TOKEN_SEP:
				if(past != code || tok->src[tok->pos - 1] != c) emit = 1;
				break;
			case TOKEN_SYM: out->data = c; emit = 1; break;
			default: emit = 1; break;
		}
		if(emit && past){
			out->type = past; out->val = tok->src + start; out->len = tok->pos - start;
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
	*out = (Token){};

	return 0;
}



size_t tokens_collect(const char* src, size_t len, Token *out, size_t cap) {
	Tokenizer tok;
	tokenizer_init(&tok, src, len);
	size_t n = 0;
	while (n < cap && token(&tok, out)) {
		out++;
		n++;
	}
	return n;
}


void tokens_symbolify(Token* tokens) {
	Token *stack[3];
	int sp = 0;

	Token *t = tokens;
	
	for(;; ++t) {
		int type = t->type;

		if(!type) break;
		else if(type == TOKEN_SYM) stack[sp++] = t;
		else if(sp < 2) sp = 0;


		if(sp == 3) {
			unsigned char data = symbol(stack[0]->val, 3);
			if(data) {
				stack[0]->data = data;
				stack[0]->len = 3;
				stack[1]->type = TOKEN_NONE;
				stack[2]->type = TOKEN_NONE;
				sp = 0;
			} else {
				data = symbol(stack[0]->val, 2);
				if(data) {
					stack[0]->data = data;
					stack[0]->len = 2;
					stack[1]->type = TOKEN_NONE;

					stack[0] = stack[2];
					sp = 1;
				} else {
					stack[0] = stack[1];
					stack[1] = stack[2];
					sp = 2;
				}
			}
		} else if(sp == 2 && type != TOKEN_SYM) {
			unsigned char data = symbol(stack[0]->val, 2);
			if(data) {
				stack[0]->data = data;
				stack[0]->len = 2;
				stack[1]->type = TOKEN_NONE;
			}
			sp = 0;
		}
	}
}


void tokens_merge(Token *tokens, size_t tokens_len, Token* stack[]) {
	// Token *stack[200];
	static Token root = {};
	int sp = 0;
	stack[sp] = &root;

	enum {_NOTHING, _PUSH, _POP, _UPGRADE, _KILL, _APPEND, _TERM, _SLUGY, _REMOVE, _ERR };

	#define in_string (cur->type == TOKEN_STR1 || cur->type == TOKEN_STR2)
	#define in_comment (cur->type == TOKEN_COMMENT1 || cur->type == TOKEN_COMMENT2)
	#define in_slug (in_string || cur->type == TOKEN_COMMENT1 || cur->type == TOKEN_COMMENT2)
	#define in_atom (cur->type == TOKEN_ID || cur->type == TOKEN_NUM)
	#define in_angle (cur->type == TOKEN_GROUP && cur->val[0] == '<')
	#define block_type CHAR_TYPE[block]
	// #define escape 

	#define kill todo = _KILL;
	#define append todo = _APPEND;
	#define term todo = _TERM;
	#define slugy todo = _SLUGY;
	#define remove todo = _REMOVE;
	#define err todo = _ERR;
	#define push(x) todo = _PUSH, payload = x;
	#define pop todo = _POP;
	#define upgrade(x) todo = _UPGRADE, payload = x;


	Token* t = tokens;
	Token* cur;
	Token* end = tokens + tokens_len;

	for(; t < end; t++) {
		cur = stack[sp];
		t->parent = cur;

		int todo = _NOTHING;
		enum TokenType payload;

		switch (t->type) {
			case TOKEN_NONE: continue;
			case TOKEN_ID:
			case TOKEN_NUM:
				if(in_atom) append
				else if(in_slug) slugy
				else push(t->type)
				break;

			case TOKEN_SEP:
				switch (t->val[0]) {
					case '\n':
						if(cur->type == TOKEN_COMMENT1) pop
						else if(in_atom) term
						else if (in_angle) kill
						else if(in_slug) slugy
					break;
					default:
						if(in_atom) pop
						else if(in_angle) kill
						else remove
					break;
				}
				break;


			case TOKEN_SYM:
				switch (t->data) {
					case '.': 
						if(in_atom) append
						else if(in_slug) slugy
						else push(TOKEN_ID)
						break;
					case '_':
						if(in_atom) append
						else if(in_slug) slugy
						else push(TOKEN_ID)
						break;
					case '[': case '{': case '(':
						if(in_atom) term
						else if(in_slug) slugy
						else if(cur->type == TOKEN_GROUP && cur->val[0] == '<') pop
						else push(TOKEN_GROUP)
						break;
					case ']': case '}': case ')':
						if(in_atom) term
						else if(in_slug) slugy
						else if(cur->type == TOKEN_GROUP && CHAR_PAIR[cur->data] == t->data) pop
						else if(in_angle) kill
						else err
						break;
					case ',':
					case ';':
						if(in_atom) term
						else if(in_slug) slugy
						break;

					case SYM_COMMENT1:
						if(in_atom) term
						else if (in_slug) slugy
						else push(TOKEN_COMMENT1)
						break;

				}
				break;
		}

		switch (todo) {
			case _APPEND: cur->len += t->len; t->type = 0; break;
			case _PUSH: t->type = payload; stack[++sp] = t; break;
			case _POP: t->type = 0; sp--;break;
			case _KILL: 
				for(Token* i = t; i != cur; i--)
					i->parent = cur->parent;
				sp--;
				cur->type = CHAR_TYPE[t->val[0]];
			break;
			case _SLUGY: t->type = TOKEN_SLUG; break;
			case _REMOVE: t->type = 0; break;
			case _TERM: t--; sp--; break;
			// case _ERR: printf("parsing error\n"); break;
			case _NOTHING: break;
		}
	}

	#undef begin
	#undef end
	#undef kill
	#undef add
	#undef add_end
	#undef slugy
	#undef remove
	#undef err
	#undef push
	#undef pop
	#undef in_string
	#undef in_comment
	#undef in_slug
	#undef in_atom
	#undef in_angle
}

size_t tokens(const char* src, size_t len, Token* list, Token** stack) {
	size_t collected = tokens_collect(src, len, list, len);
	tokens_symbolify(list);
	tokens_merge(list, collected, stack);
	return collected;
}

const char CHAR_BINARY[] = {
	// Single-char
	['+'] = 1,
	['-'] = 1,
	['*'] = 1,
	['/'] = 1,
	['%'] = 1,
	['^'] = 1,
	['&'] = 1,
	['|'] = 1,
	['<'] = 1,
	['>'] = 1,
	['='] = 1,

	// Multi-char symbols
	[SYM_DOTDOT] = 1,     // ..
	[SYM_NULLER] = 1,     // ??
	[SYM_SHL] = 1,        // <<
	[SYM_SHR] = 1,        // >>
	[SYM_LOR] = 1,        // ||
	[SYM_LAND] = 1,       // &&

	[SYM_EQEQ] = 1,       // ==
	[SYM_NEQ] = 1,        // !=
	[SYM_LTE] = 1,        // <=
	[SYM_GTE] = 1,        // >=

	[SYM_ADD_EQ] = 1,     // +=
	[SYM_SUB_EQ] = 1,     // -=
	[SYM_MUL_EQ] = 1,     // *=
	[SYM_DIV_EQ] = 1,     // /=
	[SYM_MOD_EQ] = 1,     // %=
	[SYM_XOR_EQ] = 1,     // ^=
	[SYM_AND_EQ] = 1,     // &=
	[SYM_OR_EQ] = 1,      // |=
	[SYM_SHL_EQ] = 1,     // <<=
	[SYM_SHR_EQ] = 1,     // >>=

	[SYM_ARROW] = 1,      // ->
	[SYM_SCOPE] = 1       // ::
};


struct Ast {

};

void ast(const Token* tokens, int start, size_t tokens_len) {
	enum {
		STATE_NONE,
		STATE_NAME,
		STATE_BIN_OP,
		STATE_BIN,	
	};

	int stack[256];
	stack[0] = 0;
	int sp = 0;


	for(int i = start; i < tokens_len; i++) {
		const Token* t = &tokens[i];
		
		int state = stack[sp];
		switch (state) {
			case STATE_NONE:
				if(t->type == TOKEN_ID) stack[sp++] = STATE_NAME;
				break;
			case STATE_BIN:
				if(t->type == TOKEN_SYM && CHAR_BINARY[t->data]) stack[sp] = STATE_BIN_OP;
				break;
			default:
				if(t->type == TOKEN_SYM && CHAR_BINARY[t->data]) stack[sp] = STATE_BIN;
				break;
		}

		switch (stack[sp]) {
			case STATE_BIN:
				break;
		}
	}
}




typedef unsigned long int word;
typedef unsigned char vbyte;
typedef unsigned short vshort;
typedef unsigned int vint;

#ifndef EXEC_STACK
#define EXEC_STACK 256
#endif
#ifndef exec_memcpy
#define exec_memcpy memcpy
#endif
#ifndef exec_malloc
#define exec_malloc malloc
#endif
word exec(const unsigned char *code, unsigned char mem[]) {
	typedef word (*func)(word);

	enum {
		OPC_POP, OPC_PUSH_CHAR, OPC_PUSH_SHORT, OPC_PUSH_INT, OPC_PUSH_WORD,
		OP_CALL,
		OPC_KILL,
	};
	int i = 0;

	word stack[EXEC_STACK] = {};
	int sp = 0;

	while(1) {
		unsigned char c = code[i++];
		switch (c) {
			case OPC_KILL: return stack[0];
			case OPC_POP: sp--; break;
			case OPC_PUSH_CHAR: stack[sp++] = code[i]; i += sizeof(vbyte); break;
			case OPC_PUSH_SHORT: exec_memcpy(&stack[sp++], &code[i], sizeof(vshort)); i += sizeof(vshort); break;
			case OPC_PUSH_INT: exec_memcpy(&stack[sp++], &code[i], sizeof(vint)); i += sizeof(vint); break;
			case OPC_PUSH_WORD: exec_memcpy(&stack[sp++], &code[i], sizeof(word)); i += sizeof(word); break;
			case OP_CALL: stack[sp-2] = ((func)stack[sp-2])(stack[sp-1]); --sp; break;
		}
	}
}


// void lexize(Token *tokens, size_t tokens_len, Token* stack[]) {
// 	// Token *stack[200];
// 	static Token root = {};
// 	int sp = 0;
// 	stack[sp] = &root;
// 	int pos = 0;

// 	int blocker_pos = 0;
// 	Token* blocker = NULL;
// 	char block = 0; //first char of the block

// 	//begin, start a blocker
// 	//end, end the blocker
// 	//push(x), upgrade to a scope and start it
// 	//pop, end current scope
// 	//kill, kill current scope, downgrade
// 	//add, add to the blocker
// 	//add_end, add end the blocker
// 	//slugy, set this type to TOKEN_SLUG
// 	//remove, remove this token (set to TOKEN_NONE)
// 	//err, unexpected

// 	enum {_NOTHING, _BEGIN, _END, _PUSH, _POP, _KILL, _ADD, _ADD_END, _SLUGY, _REMOVE, _ERR };

// 	#define in_string (cur->type == TOKEN_STR1 || cur->type == TOKEN_STR2)
// 	#define in_comment (cur->type == TOKEN_COMMENT1 || cur->type == TOKEN_COMMENT2)
// 	#define in_slug (in_string || cur->type == TOKEN_COMMENT1 || cur->type == TOKEN_COMMENT2)
// 	#define in_atom (cur->type == TOKEN_ID || cur->type == TOKEN_NUM)
// 	#define in_angle (cur->type == TOKEN_GROUP && cur->val[0] == '<')
// 	#define block_type CHAR_TYPE[block]
// 	// #define escape 

// 	#define begin todo = _BEGIN;
// 	#define end todo = _END;
// 	#define kill todo = _KILL;
// 	#define add todo = _ADD;
// 	#define add_end todo = _ADD_END;
// 	#define slugy todo = _SLUGY;
// 	#define remove todo = _REMOVE;
// 	#define err todo = _ERR;
// 	#define push(x) todo = _PUSH, payload = x;
// 	#define pop todo = _POP;




// 	while(pos < tokens_len) {
// 		Token* t = &tokens[pos];
// 		Token* cur = stack[sp];	

// 		t->parent = cur;

// 		int todo = _NOTHING;
// 		enum TokenType payload;

// 		switch (t->type) {
// 			case TOKEN_NONE: continue;
// 			case TOKEN_ID:
// 			case TOKEN_NUM:
// 				if(block == '$' || block == '_' || block == '.') add
// 				else if(block) end
// 				else if(in_slug) slugy
// 				else begin
// 				break;

// 			case TOKEN_SEP:
// 				if(block)end
// 				else remove
// 				break;

// 			case TOKEN_SYM:
// 				switch (t->val[0]) {
// 					case '[': case '{': case '(':
// 						if (block) end
// 						else if (in_slug) slugy
// 						else push(TOKEN_GROUP)
// 						break;
// 					case ']': case '}': case ')':
// 						if (block) end
// 						else if(in_slug) slugy
// 						if(cur->type == TOKEN_GROUP && CHAR_PAIR[cur->val[0]] == t->val[0]) pop
// 						else if(in_angle) kill
// 						else err
// 						break;
// 					case '<':
// 						if(block) end
// 						else if (in_slug) slugy
// 						else push(TOKEN_GROUP)
// 						break;
// 					case '>':
// 						if(block) end
// 						else if (in_slug) slugy
// 						else if (cur->type == TOKEN_GROUP && CHAR_PAIR[cur->val[0]] == t->val[0]) pop
// 						break;
// 					case '\'':
// 						if(block == '\\') add
// 						else if(block) end
// 						else if(cur->type == TOKEN_STR1) pop
// 						else push(TOKEN_STR1)
// 						break;
// 					case '"':
// 						if(block == '\\') add
// 						else if (block) end
// 						else if(in_comment) slugy
// 						else if(cur->type == TOKEN_STR2) pop
// 						else push(TOKEN_STR2)
// 						break;
// 					case '=':
// 						if(block) end
// 						else if(in_slug) slugy
// 						break;
// 					case '/':
// 						if(block == '*') end
// 						else if(block == '/') end
// 						else if(block) end
// 						else if(in_slug) slugy
// 						else begin
// 						break;
// 					case '_':
// 						if(block) {
// 							if(block == '_' || block == '$' || block == '.' || block_type == TOKEN_SYM || block_type == TOKEN_NUM) add
// 							else end
// 						}
// 						else if(in_slug) slugy
// 						else begin
// 						break;
// 					case '.':
// 						if(block) {
// 							if(block == '$' || block_type == TOKEN_ID) add
// 							if(block == '.') add_end
// 							else end
// 						} else begin
// 						break;
// 					case ';':
// 						if(block) end
// 						break;

						
// 				}
// 			break;
// 		}

// 		switch (todo) {
// 			case _BEGIN: blocker_pos = pos; block = t->val[0]; blocker = t; break;
// 			case _ADD: blocker->len += t->len; t->type = 0; break;
// 			case _PUSH: t->type = payload; stack[++sp] = t; break;
// 			case _POP: t->type = 0; sp--; break;
// 			case _KILL: 
// 				for (int i = pos; &tokens[i] != cur; i--)
// 					tokens[i].parent = cur->parent;
// 				sp--;
// 				cur->type = CHAR_TYPE[t->val[0]];
// 			break;
// 			case _SLUGY: t->type = TOKEN_SLUG; break;
// 			case _REMOVE: t->type = 0; break;
// 			case _ADD_END:
// 				blocker->len += t->len; t->type = 0;
// 			case _END:
				
// 				if(blocker->type == TOKEN_SYM) {
// 					blocker->data = symbol(blocker->val, blocker->len);
// 					// pos--;
// 				}

// 				if(t->type == TOKEN_SEP)
// 					pos--;

// 				switch (block) {
// 					case '_': blocker->type = TOKEN_ID; break;
// 					// case '.': blocker->type = CHAR_TYPE break;
// 				}
// 				if(block == '_') blocker->type = TOKEN_ID;

// 				block = 0;
// 				blocker = NULL;
// 				// else if(block == ',') blocker
// 			 	// block = 0;

// 				// switch (block) {
// 				// 	case '/': if (blocker->)				
// 				// }
// 				break;
// 		}
// 		pos++;
// 	}

// 	#undef begin
// 	#undef end
// 	#undef kill
// 	#undef add
// 	#undef add_end
// 	#undef slugy
// 	#undef remove
// 	#undef err
// 	#undef push
// 	#undef pop
// 	#undef in_string
// 	#undef in_comment
// 	#undef in_slug
// 	#undef in_atom
// 	#undef in_angle
// }


void parse(const Token* tokens) { 

}


// void lexize(Token *tokens, size_t tokens_len, Token* stack[], char C) {
// 	// Token *stack[200];
// 	static Token root = {};
// 	int sp = 0;
// 	stack[sp] = &root;

// 	int pos = 0;


// 	//escape, push new escape
// 	//term, ends escape

// 	enum {NORM, TERM, _PUSH, POP, APPEND, SLUGY, ERR, _UPGRADE, ESCAPE };

// 	#define in_string (cur->type == TOKEN_STR1 || cur->type == TOKEN_STR2)
// 	#define in_comment (cur->type == TOKEN_COMMENT1 || cur->type == TOKEN_COMMENT2)
// 	#define in_slug (in_string || cur->type == TOKEN_COMMENT1 || cur->type == TOKEN_COMMENT2)
// 	#define in_atom (cur->type == TOKEN_ID || cur->type == TOKEN_NUM)
// 	// #define escape 

// 	#define push todo = _PUSH, payload
// 	#define upgrade todo = _UPGRADE, payload


// 	char block[256];

// 	char out[256];
// 	out[0] = 0;
// 	int op = 0;

// 	while(pos < tokens_len) {
// 		Token* t = &tokens[pos];
// 		Token* cur = stack[sp];	

// 		t->parent = cur;

// 		int todo = NORM;
// 		enum TokenType payload;
// 		char escape = out[op];

// 		switch (t->type) {
// 			case TOKEN_NONE: continue;
// 			case TOKEN_ID:
// 			case TOKEN_NUM:
// 				if(escape == '$') upgrade = TOKEN_INTERP;
// 				else if(escape == '_') upgrade = TOKEN_ID;
// 				else if(escape == '.') upgrade = t->type;
// 				else if(in_atom) todo = APPEND;
// 				else if(in_slug) todo = SLUGY;
// 				else if(escape) todo = TERM;
// 				else todo = NORM;
// 				break;

// 			case TOKEN_SYM:
// 				if(in_atom) todo = TERM;
// 				else switch (t->val[0]) {
// 					case '[': case '{': case '(':
// 						if (in_slug) todo = SLUGY;
// 						else push = TOKEN_GROUP;
// 						break;
// 					case ']': case '}': case ')':
// 						if(in_slug) todo = SLUGY;
// 						if(cur->type == TOKEN_GROUP && CHAR_PAIR[cur->val[0]] == t->val[0]) todo = POP;
// 						else todo = ERR;
// 						break;
// 					case '\'':
// 						if(escape == '\\') todo = POP;
// 						if(cur->type == TOKEN_STR1) todo = POP;
// 						else push = TOKEN_STR1;
// 						break;
// 					case '"':
// 						if(escape == '\\') todo = POP;
// 						if(cur->type == TOKEN_STR2) todo = POP;
// 						else push = TOKEN_STR2;
// 						break;
// 					case '\\':
// 						if(in_string) todo = escape;
// 						else if (in_comment) todo = SLUGY;
// 						else todo = ERR;
// 						break;
// 					case '/':
// 						if(escape == '/') upgrade = TOKEN_COMMENT1;
// 						else if(escape == '*') todo = POP;
// 						else if (in_slug) todo = SLUGY;
// 						else todo = ESCAPE;
// 					case '*':
// 						if(in_comment) todo = ESCAPE;
// 						else if(escape == '/') upgrade = TOKEN_COMMENT2;
// 						else todo = NORM;
// 						break;
// 				}
// 			break;
// 		}

// 		switch (todo) {
// 			case _PUSH: t->type = payload; stack[++sp] = t;break;
// 			case ESCAPE: stack[++sp] = t; break;
// 			case POP: sp--; break;
// 			case TERM: pos--; sp--; break;
// 			case SLUGY: t->type = TOKEN_SLUG; break;
// 			case APPEND: cur->len += t->len; t->type = 0; break;
// 			case _UPGRADE: cur->type = payload; t->type = 0;  break;
// 			case NORM:

// 			break;
// 		}
// 		pos++;
// 	}

// 	#undef in_string
// 	#undef in_slug
// 	#undef in_atom
// 	#undef in_comment
// 	#undef escape
// }

// static inline void sum_token(Token *a, Token *b) {
// 	a->len += b->len;
// 	*b = (Token){};
// }


// static inline TokenType char_code(char c) {
// 	return CHAR_TYPE[(unsigned char)c];
// }


// int token1(Tokenizer *tok, Token *out) {
// 	TokenType prev = 0;

// 	int mpos = tok->pos, mcol = tok->col, mline = tok->line;
// 	int start = tok->pos;

// 	enum { NONE = 0, ADVANCE = 1, EMIT = 2, EMIT_ADVANCE = 3 };

// 	while (tok->pos < tok->len) {
// 		char c = tok->src[tok->pos];
// 		TokenType code = char_code(c);
// 		int todo = NONE;

// 		switch (code) {
// 		case TOKEN_SYM:
// 			if (tok->pos > start)
// 				todo = EMIT;
// 			else {
// 				todo = EMIT_ADVANCE;
// 				prev = code;
// 			}
// 			break;

// 		case TOKEN_ID:
// 		case TOKEN_NUM:
// 			if (code != prev && prev)
// 				todo = EMIT;
// 			else
// 				todo = ADVANCE;
// 			break;

// 		case TOKEN_SEP:
// 			if (tok->pos > start && tok->src[tok->pos - 1] != c)
// 				todo = EMIT;
// 			else
// 				todo = ADVANCE;
// 			break;

// 		default:
// 			return 0;
// 		}

// 		// if (todo & ADVANCE)
// 			// tokenizer_advance(tok);

// 		if (todo & EMIT) {
//       		*out = (Token){.type = prev, .val = tok->src + start, .len = tok->pos - start, .pos = mpos, .line = mline, .col = mcol};
// 			return 1;
// 		}

// 		prev = code;
// 	}

// 	if (prev) {
//     	*out = (Token){.type = prev, .val = tok->src + start, .len = tok->pos - start, .pos = mpos, .line = mline, .col = mcol};
// 		return 1;
// 	}

// 	*out = (Token){};
// 	return 0;
// }

void lexize1(Token *tokens, size_t tokens_len, Token* stack[]) {
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
			else if (cur->type == TOKEN_COMMENT1 || t->val == '\n') do_pop = 1;
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



// Node* node_alloc() {return malloc(sizeof(Node));}
// void parse(const Token* tokens, size_t tokens_len, Node* out, Node* stack[], Node* (*alloc)()) {
// 	int sp = 0;
// 	int pos = 0;


	
// 	for(;;pos++) {
// 		if(pos >= tokens_len) break;

// 		int do_push = 0;

// 		const Token* t = &tokens[pos];

// 		switch (t->type) {
// 			case TOKEN_NONE: continue;
// 			case TOKEN_ID: do_push = 1; break;
// 			case TOKEN_SYM: do_push = 1; break;
// 		}


// 		if(do_push) {
// 			stack[sp++] = alloc();
// 		}
// 	}
// 	printf("sp is %d\n", sp);
// }


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
	printf("%-3s @%d:%d ", token_type_name(t->type), t->line, t->col);

	printf("\"");


	fwrite(t->val, 1, t->len, stdout);
	printf("\"");

	if(t->data)
		printf(" (%i)", t->data);
	printf("\n");
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

	char* fbuf = NULL;
	// long flen = _read_file("ex/index.jon", &fbuf);
	// printf("fuck %ld\n", flen);
	
	long flen;

	
	
	// const char src[] = "if while x{0}+c";
	// tokenizer_init(&tok, src, sizeof(src) - 1);

	
	fbuf = "a + b * c + (d * x)";
	printf("%s\n", fbuf);
	flen = strlen(fbuf);
	// tokenizer_init(&tok, fbuf, flen);
	
	Token* stack[200];
	Token* list = NULL;

	list = malloc(sizeof(Token) * (flen + 1));
	size_t len = tokens(fbuf, flen, list, stack);


	
	// size_t len = tokens_collect(&tok, list, 1000 * 1000);
	// tokens_symbolify(list);
	// tokens_merge(list, len, stack);
	
	
	// static	Node* node_stack[1000];

	// Node parsed;
	// parse(list, len, &parsed, node_stack);
	

	// Parser p;
	// init_parser(&p, list, len);
	// int parsed = parse_bin(&p, 0, &nodes[0], &nodes[1], &nodes[2]);

	// char buf[1000];

	// print_node(&nodes[0], buf);

	// if(parsed)
	// printf("%s\n", buf);

	// astify(list, 0, len, NULL);
// nodes[0] = (Node){.t=AST_BIN,.bin={OP_MUL,&nodes[1],&nodes[2]}};
// nodes[1] = (Node){.t=AST_ID,.id={"a",1}};
// nodes[2] = (Node){.t=AST_BIN,.bin={OP_SUM,&nodes[3],&nodes[4]}};
// nodes[3] = (Node){.t=AST_ID,.id={"b",1}};
// nodes[4] = (Node){.t=AST_ID,.id={"c",1}};

// 	char buf[1000];
// 	print_node(&nodes[0], buf);
// 	printf("%s\n", buf);


	for (int i = 0; i < len; i++) {
		print_token(&list[i]);
	}

	printf("last is %i\n", list[flen+1].type);

}
#endif