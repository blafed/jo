#define JO_USE_IMPL

#ifndef JO_H
#define JO_H
#include <stddef.h>

void* jo_new(unsigned long size);                           // heap alloc
void jo_del(void* ptr);                                     // heap free
void* jo_exp(void* ptr, unsigned long size);                // heap expand/realloc
void jo_cpy(void* dst, const void* src, unsigned long len); // mem cpy
void jo_echo(const char* s, size_t len);                    //print
int jo_cmp(const void* a, const void* b, size_t len);       //memcmp

size_t jo_fmt(char* buf, const char* fmt, ...); //sprintf

int jo_stry_uint(unsigned long i, char* buf, int base);
int jo_stry_float(double f, char* buf);

enum TokenType : unsigned char {
    TOK_NONE = 0,

    TOK_FRAG = 1, //a piece inside comment or string
    TOK_GROUP = 2,
    TOK_STRING = 3,
    TOK_COMMENT_LINE = 4,
    TOK_COMMENT_BLOCK = 5,

    TOK_ID = 'A',
    TOK_NUM = '0',
    TOK_EXPONENT = 'e',
    TOK_NEWLINE = '\n',

    TOK_EMPTY = ' ',
    TOK_DOT = '.',
    TOK_TERM = ',',

    //double character symbols
    TOK_SLASH_SLASH = 128,
    TOK_SLASH_STAR,
    TOK_STAR_SLASH,

    //keywords
    TOK_NIL = 192,
    TOK_FALSE,
    TOK_TRUE,
};

enum ValueType {
    VAL_NONE,

    VAL_INT,
    VAL_BOOL,
    VAL_FLOAT,

    VAL_STR,
    VAL_ID,

    VAL_ARR,
    VAL_OBJ,
};

struct Value {
    enum ValueType type;
    int len;
    union {
        long i;
        double f;
        char* s;
        struct Value* p;
    };
};

struct Token {
    enum TokenType type;
    int pos, len;
    int line, col;
    const char* val;
    int depth;
};

typedef struct Token Token;
typedef struct Value Value;

int tokenize(const char* src, Token* list);
Token* organize(Token* list, Token* stack[]);
int serialize(Value value, char* out);
Token* parse(Token* tokens, Value* obj);

const char* tokentype_name(enum TokenType type);
void token_print(const Token* t);
#endif

const Value VALUE_NIL = {};
const Value VALUE_FALSE = {.type = VAL_BOOL, .i = 0};
const Value VALUE_TRUE = {.type = VAL_BOOL, .i = 1};
const Value VALUE_ZERO = {.type = VAL_INT, .i = 0};

int value_cmp(Value a, Value b);
int slice_cmp(Value a, Value b, int start, int len);
int value_cmp(Value a, Value b);
Value value_obj(void);
Value value_id(const char* s);
Value value_str(const char* s, int len);
Value value_float(double d);
Value value_int(long i);
Value value_index(Value v, int index);
Value value_key(Value v, const char* key);

#ifdef JO_USE_IMPL

unsigned char token_type(const char* s) {
    unsigned char c = s[0];
    if (c >= 'A' && c <= 'Z' || c >= 'a' && c <= 'z') return TOK_ID;
    if (c >= '0' && c <= '9') return TOK_NUM;
    switch (c) {
        case ' ':
        case '\t': return TOK_EMPTY;
        case '\n': return TOK_NEWLINE;
        case '/':
            switch (s[1]) {
                case '/': return TOK_SLASH_SLASH;
                case '*': return TOK_SLASH_STAR;
            }
            break;
        case '*':
            switch (s[1]) {
                case '/': return TOK_STAR_SLASH;
            }
            break;
    }

    return c;
}

void advance_cursor(const char* src, int* pos, int* line, int* col) {
    *col = *col + 1;
    if (src[*pos] == '\n') {
        *line = *line + 1;
        *col = 0;
    }
    *pos = *pos + 1;
}

enum TokenType keyword(const char* s, int len) {
    switch (s[0]) {
        case 'n':
            if (!jo_cmp(s, "nil", 3)) return TOK_NIL;
            break;
        case 'f':
            if (!jo_cmp(s, "false", 5)) return TOK_FALSE;
            break;
        case 't':
            if (!jo_cmp(s, "true", 4)) return TOK_TRUE;
            break;
    }
    return TOK_ID;
}

int keyword_str(enum TokenType type, char* out) {
    switch (type) {
        case TOK_NIL:   jo_cpy(out, "nil", 3); return 3;
        case TOK_TRUE:  jo_cpy(out, "true", 4); return 4;
        case TOK_FALSE: jo_cpy(out, "false", 5); return 5;
        default:        return 0;
    }
}

int tokenize(const char* src, Token* list) {
    int pos = 0, line = 0, col = 0;
    int i = 0;

    char prev = 0;
    while (1) {
        int todo;
        char c = src[pos];
        unsigned char type = token_type(src + pos);

        if (prev && type != prev) {
            prev = 0;
            i++;
        }

        if (!type)
            break;

        if (!prev) {
            list[i] = (Token){.type = type, .col = col, .line = line, .pos = pos, .val = src + pos};
            if (type >= 128) {
                list[i].len++;
                advance_cursor(src, &pos, &line, &col);
            }
        }

        list[i].len++;
        advance_cursor(src, &pos, &line, &col);

        prev = type;
    }
    return i;
}

static Token ROOT_TOKEN = {.type = 100, .depth = 0};

Token* organize(Token* list, Token* stack[]) {
    enum {
        NOTHING,
        APPEND, //append to current
        TERM,   //terminate current
        PUSH,
        POP, //terminate current and advance
        REMOVE,
        FRAG,   //turn this into a frag!
        BECOME, //change type of this
        INSERT, //terminate current and change type of this
        ERROR,
    };

    // clang-format off
#define append todo = APPEND;
#define term todo = TERM;
#define push(x) {todo = PUSH; payload = x;}
#define pop todo = POP;
#define remove todo = REMOVE;
#define frag todo = FRAG;
#define err todo = ERROR;
#define become(x) {todo = BECOME; payload = x;}
#define insert(x) {todo = INSERT; payload = x;}

    Token* cur;
    int sp = 0;

    int todo = 0;
    int payload = 0;

    static Token IDLE = {};
    IDLE.type = 0;
    stack[sp] = &IDLE;

    for (Token* t = list; t->type ; t++) {
        todo = 0;
        cur = stack[sp];
        t->depth = cur->depth + 1;
        
        unsigned char type = t->type;

        switch (cur->type) {
            case TOK_STRING:
                switch (type) {
                    case '\'': pop break;
                    default: frag break;
                }
                break;
            case TOK_COMMENT_BLOCK:
                if(type == TOK_STAR_SLASH) pop
                else remove
                break;
            case TOK_COMMENT_LINE:
                if(type == TOK_NEWLINE) pop
                else remove
                break;

            case TOK_NUM:
                if(type == TOK_ID) {
                    if(t->val[0] == 'e' || t->val[0] == 'E') insert(TOK_EXPONENT)
                    else append
                }
                else if(type == '_') append
                else if(type == TOK_NUM) append
                else term
                break;
            case TOK_ID:
                if(type == TOK_NUM || type == TOK_ID) append
                else if(type == '_') append
                else term
                break;
            case TOK_EXPONENT:
                if(type == '-' || type == '+') append
                else if(type == TOK_NUM) append
                else term
                break;
                
            default:
                switch (type) {
                    case '{': push(TOK_GROUP); break;
                    case '}': if(cur->type == TOK_GROUP) pop else err; break;
                    case ',': become(TOK_TERM); break;
                    case '.': become(TOK_DOT); break;
                    case '\'': push(TOK_STRING); break;
                    case TOK_NEWLINE: become(TOK_TERM); break;
                    case TOK_STRING: push(TOK_STRING); break;
                    case TOK_SLASH_SLASH: push(TOK_COMMENT_LINE); break;
                    case TOK_SLASH_STAR: push(TOK_COMMENT_BLOCK); break;
                    case TOK_EMPTY: remove break;
                    case TOK_NUM:
                    case TOK_ID: push(type); break;
                    case '+': 
                    case '-': break;
                    case '_':  become(TOK_ID) break;
                    default: err
                }
                break;
        }

        switch (todo) {
            case APPEND: cur->len += t->len; t->type = TOK_EMPTY; break;
            case TERM: t->depth--; sp--; t--; break;
            case PUSH: t->type = payload; stack[++sp] = t;  break;
            case POP: t->type = TOK_EMPTY; sp--; break;
            case REMOVE: t->type = TOK_EMPTY; break;
            case FRAG: t->type = TOK_FRAG; break;
            case BECOME: t->type = payload; break;
            case INSERT: t->type = payload; t->depth--; stack[sp] = t; break;
            case ERROR: return  t;
        }
    }

    // clang-format on

    for (Token* t = list; t->type; t++) {
        //remove comments
        if (t->type == TOK_COMMENT_LINE || t->type == TOK_COMMENT_BLOCK)
            t->type = TOK_EMPTY;
        //find keywords
        if (t->type == TOK_ID)
            t->type = keyword(t->val, t->len);
    }

#undef append
#undef term
#undef push
#undef pop
#undef remove
#undef frag
#undef err
#undef become

    return NULL;
}

Value* value_exp(Value* v, int len) {
    return jo_exp(v, sizeof(Value) * len);
}

char* str_alloc(const char* from, int len) {
    //TODO
    char* out = jo_new(len + 1);
    jo_cpy(out, from, len);
    out[len] = 0;
    return out;
}

int serialize(Value v, char* out) {
    char* start = out;
    switch (v.type) {
        case VAL_INT: {
            int sign = 1;
            if (v.i < 0) {
                sign = -1;
                *out = '-';
                out++;
            }
            out += jo_stry_uint(v.i * sign, out, 10);
        } break;
        case VAL_FLOAT:
            out += jo_stry_float(v.f, out);
            break;
        case VAL_STR:
            *out = '\'';
            out++;
            jo_cpy(out, v.s, v.len);
            out += v.len;
            *out = '\'';
            out++;
            break;
        case VAL_ID:
            jo_cpy(out, v.s, v.len);
            out += v.len;
            break;
        case VAL_ARR:
            *out = '{';
            out++;
            for (int i = 0; i < v.len; i++) {
                const Value* p = &v.p[i];
                if (p != v.p) {
                    *out = ',';
                    out++;
                }
                out += serialize(*p, out);
            }
            *out = '}';
            out++;
            break;
        case VAL_OBJ:
            *out = '{';
            out++;
            for (int i = 0; i < v.len; i += 2) {
                const Value* p = &v.p[i];
                if (p != v.p) {
                    *out = ',';
                    out++;
                }
                out += serialize(*p, out);
                *out = ' ';
                out++;
                out += serialize(*(p + 1), out);
            }
            *out = '}';
            out++;
            break;
        case VAL_NONE: out += keyword_str(TOK_NIL, out); break;
        case VAL_BOOL: out += keyword_str(v.i + TOK_FALSE, out); break;
    }
    return out - start;
}

static inline int digit_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

int str_to_uint(unsigned long* out, const char* s, size_t n, int base) {
    unsigned long v = 0;
    size_t i = 0;
    for (; i < n; i++) {
        char c = s[i];
        if (c == '_') continue;

        int d = digit_val(c);
        if (d < 0) break;

        v = v * base + d;
    }

    *out = v;
    return i;
}

int str_to_int(long* out, const char* s, size_t n, int base) {
    int sign = 1;
    int diff = 0;
    if (s[0] == '+') {
        s++;
        diff = 1;
    } else if (s[0] == '-') {
        s++;
        sign = -1;
        diff = 1;
    }

    unsigned long v;
    int dec = str_to_uint(&v, s, n - diff, base);
    *out = v * sign;
    return dec + diff;
}

int str_to_fraction(double* out, const char* s, size_t n) {
    unsigned long f;
    int dec = str_to_uint(&f, s, n, 10);
    long x = 1;
    for (int d = 0; d < dec; d++)
        x *= 10;
    *out = (double)f / x;
    return dec;
}

static inline Token* skip_empty(Token* tok) {
    while (tok->type == TOK_EMPTY) tok++;
    return tok;
}

static inline Token* skip_term(Token* tok) {
    while (tok->type == TOK_TERM || tok->type == TOK_EMPTY) tok++;
    return tok;
}

static inline Token* next(Token* tok) {
    tok++;
    return skip_empty(tok);
}

Token* parse_exponent(Token* start, Value* out) {
    Token* t = start;
    if (t->type != TOK_EXPONENT)
        return start;

    long val;
    if (str_to_int(&val, t->val + 1, t->len - 1, 10) != t->len - 1)
        return start;
    for (int i = 0; i < val; i++) out->f *= 10;
    for (int i = val; i < 0; i++) out->f /= 10;

    out->type = VAL_FLOAT;
    t = next(t);
    return t;
}

Token* parse_dec(Token* start, Value* out) {
    Token* t = start;
    if (t->type != TOK_DOT)
        return start;
    t = next(t);
    if (t->type != TOK_NUM)
        return start;

    double f;
    if (str_to_fraction(&f, t->val, t->len) != t->len)
        return start;

    t = next(t);
    out->f += f;
    out->type = VAL_FLOAT;

    if (t->type == TOK_EXPONENT)
        t = parse_exponent(t, out);

    return t;
}

Token* parse_int(Token* start, Value* out) {
    Token* t = start;
    unsigned long l = 0;
    if (str_to_uint(&l, t->val, t->len, 10) == t->len) {
        *out = (Value){.i = l, .type = VAL_INT};
        t = next(t);
    }
    return t;
}

Token* parse_num(Token* start, Value* out) {
    Token* t = start;

    int sign = 1;
    if (t->type == '-') {
        sign = -1;
        t = next(t);
    } else if (t->type == '+')
        t = next(t);

    t = parse_int(t, out);
    if (t->type == TOK_DOT) {
        unsigned long l = out->i;
        out->f = l;
        Token* n = parse_dec(t, out);
        if (n == t) //check success
            out->i = l;
        else
            t = n;
    } else if (t->type == TOK_EXPONENT) {
        unsigned long l = out->i;
        out->f = l;
        Token* n = parse_exponent(t, out);
        if (n == t) //check success
            out->i = l;
        else
            t = n;
    }
    if (out->type == VAL_INT)
        out->i *= sign;
    else if (out->type == VAL_FLOAT)
        out->f *= sign;
    return t;
}

Token* parse_id(Token* start, Value* out) {
    Token* t = start;
    if (t->type == TOK_ID) {
        *out = (Value){.type = VAL_ID, .s = str_alloc(start->val, start->len), .len = start->len};
        t = next(t);
    }
    return t;
}

Token* parse_str(Token* start, Value* out) {
    Token* t = start;
    if (t->type != TOK_STRING)
        return t;

    *out = (Value){.type = VAL_STR, .s = jo_exp(NULL, 1), .len = 0};
    out->s[out->len] = 0;

    t = next(t);
    for (; t->type == TOK_FRAG && start->depth + 1 == t->depth; t = next(t)) {
        out->s = jo_exp((char*)out->s, out->len + t->len + 1);
        jo_cpy(out->s + out->len, t->val, t->len);
        out->len += t->len;
        out->s[out->len] = 0;
    }
    return t;
}

Token* parse_any(Token* tok, Value* out);

Token* parse_kv(Token* start, Value* k, Value* v) {
    Token* t = start;
    if (t->type == TOK_ID) {
        t = parse_id(t, k);
        t = skip_empty(t);
        t = parse_any(t, v);
    } else {
        k->type = 0;
        v->type = 0;
    }
    return t;
}

Token* parse_arr(Token* start, Value* out) {
    Token* t = start;
    int cap = out->len;

    if (t->type == TOK_TERM)
        t = skip_term(t);

    for (;;) {
        t = skip_empty(t);

        Value el = {};
        Token* n = parse_any(t, &el);
        if (n == t || n == NULL) //check valid
            break;
        else
            t = n;

        if (cap == out->len) {
            cap = cap * 2 + 2;
            out->p = value_exp(out->p, cap);
        }
        out->p[out->len] = el;
        out->len++;

        if (t->type == TOK_TERM && t->depth == start->depth)
            t = skip_term(t);
        else
            break;
    }
    return t;
}

Token* parse_arr_kv(Token* start, Value* out) {
    Token* t = start;
    int cap = out->len;

    if (t->type == TOK_TERM)
        t = skip_term(t);

    for (;;) {
        t = skip_empty(t);

        Value k = {}, v = {};
        Token* n = parse_kv(t, &k, &v);
        if (t == n || n == NULL) //check success
            break;
        else
            t = n;

        if (cap == out->len) {
            cap = cap * 2 + 2;
            out->p = value_exp(out->p, cap);
        }

        out->p[out->len] = k;
        out->p[out->len + 1] = v;
        out->len += 2;

        t = skip_empty(t);

        if (t->type == TOK_TERM && t->depth == start->depth)
            t = skip_term(t);
        else
            break;
    }
    return t;
}

int is_kv(Token* tok) {
    if (tok->type == TOK_ID) {
        Token* t = next(tok);
        if (t->type != TOK_TERM && t->type != TOK_NONE && t->depth == tok->depth)
            return 1;
    }
    return 0;
}

Token* parse(Token* start, Value* out) {
    Token* t = start;
    t = skip_empty(t);
    if (is_kv(t)) {
        out->type = VAL_OBJ;
        t = parse_arr_kv(t, out);
    } else {
        out->type = VAL_ARR;
        t = parse_arr(t, out);
    }
    t = skip_empty(t);
    return t;
}

Token* parse_group(Token* start, Value* out) {
    Token* t = start;
    if (t->type == TOK_GROUP) {
        t = next(t);
        t = parse(t, out);
    }
    return t;
}

Token* parse_any(Token* tok, Value* out) {
    switch ((unsigned char)tok->type) {
        case TOK_NIL:      *out = VALUE_NIL; return tok + 1;
        case TOK_FALSE:
        case TOK_TRUE:     *out = (Value){.type = VAL_BOOL, .i = tok->type - TOK_FALSE}; return tok + 1;
        case TOK_NUM:
        case '+':
        case '-':          return parse_num(tok, out);
        case TOK_DOT:      return parse_dec(tok, out);
        case TOK_EXPONENT: return parse_exponent(tok, out);
        case TOK_ID:       return parse_id(tok, out);
        case TOK_STRING:   return parse_str(tok, out);
        case TOK_GROUP:    return parse_group(tok, out);
        default:           return NULL;
    }
}

const char* tokentype_name(enum TokenType type) {
    switch ((unsigned char)type) {
        case TOK_NONE:          return "NONE";
        case TOK_ID:            return "ID";
        case TOK_NUM:           return "NUM";
        case TOK_EMPTY:         return "   ";
        case TOK_GROUP:         return "GROUP";
        case TOK_STRING:        return "STRING";
        case TOK_COMMENT_LINE:  return "COMMENT";
        case TOK_COMMENT_BLOCK: return "COMMENT";
        case TOK_FRAG:          return "FRAG";
        case TOK_TERM:          return "TERM";
        case TOK_DOT:           return "DOT";
        case TOK_NEWLINE:       return "NL";
        case TOK_EXPONENT:      return "EXPONENT";
        case '-':
        case '+':               return "UNARY";
        default:                return "OTHER";
    }
}

void token_print(const Token* t) {
#define COL_RESET  "\x1b[0m"
#define COL_GREEN  "\x1b[32m"
#define COL_PURPLE "\x1b[45m"

    if (!t->type)
        return;

    char buf[256];
    int p = 0;

    const char* name = tokentype_name(t->type);
    for (int i = 0; i < t->depth; i++)
        p += jo_fmt(buf + p, "| ");

    p += jo_fmt(buf + p, COL_PURPLE "%-3s" COL_RESET " @%d:%d ", name, t->line + 1, t->col + 1);

    p += jo_fmt(buf + p, COL_GREEN);

    if (t->val[0] == '\n')
        p += jo_fmt(buf + p, "\\n");
    else
        p += jo_fmt(buf + p, "%.*s", t->len, t->val);
    p += jo_fmt(buf + p, COL_RESET);
    jo_echo(buf, p);
    jo_echo("\n", 1);

#undef COL_RESET
#undef COL_GREEN
#undef COL_PURPLE
}

int slice_cmp(Value a, Value b, int start, int len) {
    int stepA = a.type - VAL_ARR + 1;
    int stepB = b.type - VAL_ARR + 1;

    if (a.len < start + len || b.len < start + len)
        return -1;

    int end = start + len;

    while (start != end) {
        if (value_cmp(a.p[start * stepA], b.p[start * stepB]))
            return 1;

        start++;
    }

    return 0;
}

int value_cmp(Value a, Value b) {
    switch (a.type) {
        case VAL_NONE:
            switch (b.type) {
                case VAL_NONE: return 0;
                default:       return 1;
            }

        case VAL_BOOL:
        case VAL_INT:
            switch (b.type) {
                case VAL_BOOL:
                case VAL_INT:   return a.i != b.i;
                case VAL_FLOAT: return a.i != b.f;
                default:        return 1;
            }
        case VAL_FLOAT:
            switch (b.type) {
                case VAL_BOOL:
                case VAL_INT:   return a.f != b.i;
                case VAL_FLOAT: return a.f != b.f;
                default:        return 1;
            }
        case VAL_ARR:
        case VAL_OBJ:
            switch (b.type) {
                case VAL_ARR:
                case VAL_OBJ:
                    return a.len != b.len || slice_cmp(a, b, 0, a.len);
                default: return 1;
            }
        case VAL_STR:
        case VAL_ID:
            switch (b.type) {
                case VAL_STR:
                case VAL_ID:
                    return a.len == b.len && jo_cmp(a.s, b.s, a.len) != 0;
                default: return 1;
            }

        default:
            return a.type == b.type && a.i == b.i;
    }
}
Value value_obj() {
    return (Value){.type = VAL_OBJ};
}
Value value_id(const char* s) {
    int len = 0;
    while (s[len]) len++;
    return (Value){.type = VAL_ID, .s = str_alloc(s, len), .len = len};
}
Value value_str(const char* s, int len) {
    if (!len)
        while (s[len]) len++;
    return (Value){.type = VAL_STR, .s = str_alloc(s, len), .len = len};
}
Value value_float(double d) { return (Value){.type = VAL_FLOAT, .f = d}; }
Value value_int(long i) { return (Value){.type = VAL_INT, .i = i}; }
Value value_index(Value v, int i) {
    switch (v.type) {
        case VAL_OBJ: return v.p[i * 2 + 1];
        case VAL_ARR: return v.p[i];
        default:      return VALUE_NIL;
    }
}
Value value_key(Value v, const char* key) {
    if (v.type == VAL_OBJ)
        for (int i = 0; i < v.len; i += 2) {
            if (!jo_cmp(v.p[i].s, key, v.p[i].len))
                return v.p[i + 1];
        }
    return VALUE_NIL;
}

#endif

#ifdef JO_USE_LINUX
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
void* jo_new(unsigned long size) { return malloc(size); }
void jo_del(void* ptr) { free(ptr); }
void* jo_exp(void* ptr, unsigned long size) { return realloc(ptr, size); }
void jo_echo(const char* s, size_t len) { fwrite(s, 1, len, stdout); }
int jo_cmp(const void* a, const void* b, size_t len) { return memcmp(a, b, len); }
void jo_cpy(void* dst, const void* src, unsigned long len) { memcpy(dst, src, len); }
size_t jo_fmt(char* buf, const char* fmt, ...) {
    va_list args;

    va_start(args, fmt);
    int n = vsprintf(buf, fmt, args);
    va_end(args);

    return n < 0 ? 0 : (size_t)n;
}
// clang-format off
int jo_stry_uint(unsigned long i, char* buf, int base) {
    return sprintf(buf, base == 16 ? "%lX" : base == 8 ? "%lo" : "%lu", i);
}
int jo_stry_float(double f, char* buf) {
    int n = sprintf(buf, "%g", f);

    if (!strchr(buf, '.') && !strchr(buf, 'e') && !strchr(buf, 'E'))
    {
        buf[n++] = '.';
        buf[n++] = '0';
        buf[n] = '\0';
    }

    return n;
}
// clang-format on
#endif