#include <stddef.h>

void* jo_new(unsigned long size);                           // heap alloc
void jo_del(void* ptr);                                     // heap free
void* jo_exp(void* ptr, unsigned long size);                // heap expand/realloc
void jo_cpy(void* dst, const void* src, unsigned long len); // mem cpy
void jo_echo(const char* s, size_t len);                    //print

size_t jo_fmt(char* buf, const char* fmt, ...); //sprintf

// int jo_parse_uint(unsigned long* out, const char* s, int n, int base);
int jo_stry_uint(unsigned long i, char* buf, int base);
int jo_stry_float(double f, char* buf);

enum TokenType {
    TOK_NONE,

    TOK_ID,
    TOK_NUM,
    TOK_SYM,
    TOK_SEP,

    //computed at second pass

    TOK_EMPTY, // -> removed
    TOK_FRAG,  //a piece inside comment or string
    TOK_DOT,   //.
    TOK_OP,
    TOK_TERM, //terminator like line

    TOK_GROUP,
    TOK_STRING,
    TOK_COMMENT,
};

enum ValueType {
    VAL_NONE,

    VAL_INT,
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
    unsigned char data;
    int pos, len;
    int line, col;
    const char* val;
    int depth;
};

struct Cursor {
    int pos;
    int line, col;
};

typedef struct Token Token;
typedef struct Value Value;
typedef struct Cursor Cursor;

int tokenize(const char* src, struct Token* list);
struct Token* organize(struct Token* list, struct Token* stack[]);
int serialize(Value value, char* out);
const struct Value* parse(struct Token* list, int* stack, int* out_len);

enum {
    TOK_SLASH_SLASH = 128,
};

const char* tokentype_name(enum TokenType type);

static char token_type(char c) {
    if (c >= 'A' && c <= 'Z' || c >= 'a' && c <= 'z') return TOK_ID;
    if (c >= '0' && c <= '9') return TOK_NUM;
    if (c == ' ' || c == '\t' || c == '\n') return TOK_SEP;
    if (c < 32 || c > 127) return TOK_NONE; //non printable chars, but we took \n and \t before already
    return TOK_SYM;
}

static unsigned char sym_type(const char* seq, int len) {
    switch (seq[0]) {
        case '/':
            if (len == 2 && seq[1] == '/')
                return TOK_SLASH_SLASH;
    }
    return seq[0];
}

int next(const char* src, struct Cursor* cur, struct Token* out) {
    char prev = 0;
    while (1) {
        char c = src[cur->pos];
        char type = token_type(c);
        if (prev && type != prev) break;
        if (!prev)
            *out = (struct Token){.type = type, .col = cur->col, .line = cur->line, .pos = cur->pos, .val = src + cur->pos, .data = c};
        out->len++;

        prev = type;
        cur->pos++;
        if (c == '\n') {
            cur->line++;
            cur->col = 0;
        } else
            cur->col++;

        if (!type) break;
        if (type == TOK_SYM) {
            out->data = sym_type(out->val, 2);
            if (out->data != out->val[0]) {
                cur->pos++;
                out->len = 2;
            }
            break;
        }
    }
    return out->type;
}

int tokenize(const char* src, struct Token* list) {
    struct Cursor cursor = {};
    int i = 0;
    while (next(src, &cursor, &list[i]))
        i++;
    return i;
}

static struct Token ROOT_TOKEN = {.type = 100, .depth = 0};

struct Token* organize(struct Token* list, struct Token* stack[]) {
    enum {
        NOTHING,
        APPEND, //append to current
        TERM,   //terminate current
        PUSH,
        POP, //terminate current and advance
        REMOVE,
        FRAG, //turn this into a frag!
        BECOME,
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


#define in_atom cur->type == TOK_ID || cur->type == TOK_NUM
#define in_group cur->type == TOK_GROUP
#define in_string cur->type == TOK_STRING
#define in_comment cur->type == TOK_COMMENT
#define in_fragy (in_string || in_comment)

    struct Token* cur;
    int sp = 0;

    int todo = 0;
    int payload = 0;

    stack[sp] = &ROOT_TOKEN;

    for (struct Token* t = list; t->type ; t++) {
        todo = 0;
        cur = stack[sp];
        t->depth = cur->depth + 1;

        switch (t->type) {
        case TOK_SEP:
            if(t->data == '\n') {
                if(in_comment) pop
                else if(in_fragy) frag
                else if(in_atom) term
                else become(TOK_TERM)  
            } else {
                if(in_fragy) frag
                else if(in_atom) term
                else remove
            }
            break;
        case TOK_ID:
        case TOK_NUM:
            if(in_fragy) frag
            else if(in_atom) append
            else push(t->type);
            break;
        case TOK_SYM:
            switch (t->data) {
                case '{':
                    if(in_atom) term
                    else if(in_fragy) frag
                    else push(TOK_GROUP)
                    break;
                case '}':
                    if(in_group) pop
                    else if(in_atom) term
                    else if(in_fragy) frag
                    else err
                    break;
                case '\'':
                    if(in_atom) term
                    else if(in_string) pop
                    else if(in_fragy) frag
                    else push(TOK_STRING)
                    break;
                case '_':
                    if(in_atom) append
                    else if(in_fragy) frag
                    else push(TOK_ID)
                    break;
                case '.':
                    if(in_atom) term
                    else if(in_fragy) frag
                    else become(TOK_DOT)
                    break;
                case TOK_SLASH_SLASH:
                    if(in_atom) term
                    else if(in_fragy) frag
                    else push(TOK_COMMENT)
                    break;
                case ',':
                    if(in_atom) term
                    else if(in_fragy) frag
                    else become(TOK_TERM)
                    break;
                case '-':
                case '+':
                        if(in_atom) term
                        else if(in_fragy) frag
                        else become(TOK_OP);
                        break;
                default:
                    if(in_fragy) frag
                    else err
            }
            break;
            default:
                if(in_fragy) frag
                else err 
        }

        switch (todo) {
            case APPEND: cur->len += t->len; t->type = TOK_EMPTY; break;
            case TERM: t->depth--; sp--; t--; break;
            case PUSH: t->type = payload; stack[++sp] = t;  break;
            case POP: t->type = TOK_EMPTY; sp--; break;
            case REMOVE: t->type = TOK_EMPTY; break;
            case FRAG:
                // if((t-1)->type == TOK_FRAG) {(t-1)->len += t->len; t->type = TOK_EMPTY;}
                if(in_comment) t->type = TOK_EMPTY; 
                else t->type = TOK_FRAG;
                break;
            case BECOME: t->type = payload; break;
            case ERROR: return  t;
        }
    }

    struct Token* c = NULL;
    for(struct Token* t = list; t->type; t++) {
        if(t->type == TOK_COMMENT) t->type = TOK_EMPTY;
        if(t->type == TOK_TERM) {
            if(c && c->depth == t->depth) t->type = TOK_EMPTY;
            else c = t;
        }
        else c = NULL;
    }

    // clang-format on

#undef append
#undef term
#undef push
#undef pop
#undef remove
#undef frag
#undef err
#undef become
#undef in_atom
#undef in_group
#undef in_string
#undef in_comment
#undef in_fragy

    return NULL;
}

struct Value* value_exp(struct Value* v, int len) {
    return jo_exp(v, sizeof(struct Value) * len);
}

char* str_alloc(const char* from, int len) {
    //TODO
    char* out = jo_new(len + 1);
    jo_cpy(out, from, len);
    out[len] = 0;
    return out;
}

int serialize(Value v, char* out) {
    //TODO optimize sprintfs
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
        case VAL_NONE:
            break;
    }
    return out - start;
}

static inline int digit_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

int parse_uint(unsigned long* out, const char* s, size_t n, int base) {
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

int parse_fracture(double* out, const char* s, size_t n) {
    unsigned long f;
    int dec = parse_uint(&f, s, n, 10);
    long x = 1;
    for (int d = 0; d < dec; d++)
        x *= 10;
    *out = (double)f / x;
    return dec;
}

static inline struct Token* skip_empty(struct Token* tok) {
    while (tok->type == TOK_EMPTY) tok++;
    return tok;
}

static inline struct Token* get_next(struct Token* tok) {
    tok++;
    return skip_empty(tok);
}

static inline struct Token* get_prev(struct Token* tok) {
    tok--;
    while (tok->type == TOK_EMPTY) tok--;
    return tok;
}

struct Token* parse_dec(struct Token* start, struct Value* out) {
    struct Token* t = start;
    if (t->type != TOK_DOT)
        return start;
    t = get_next(t);
    if (t->type != TOK_NUM)
        return start;

    double f;
    if (parse_fracture(&f, t->val, t->len) != t->len)
        return start;

    t = get_next(t);

    out->f += f;
    out->type = VAL_FLOAT;
    return t;
}

struct Token* parse_int(struct Token* start, struct Value* out) {
    struct Token* t = start;
    unsigned long l = 0;
    if (parse_uint(&l, t->val, t->len, 10) == t->len) {
        *out = (Value){.i = l, .type = VAL_INT};
        t = get_next(t);
    }
    return t;
}

struct Token* parse_num(struct Token* start, struct Value* out) {
    struct Token* t = start;
    int sign = 1;
    if (t->type == TOK_OP) {
        if (t->data == '-')
            sign = -1;
        t = get_next(t);
    }
    t = parse_int(t, out);
    if (t->type == TOK_DOT) {
        unsigned long l = out->i;
        out->f = l;
        struct Token* n = parse_dec(t, out);
        if (n == t)
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

struct Token* parse_id(struct Token* start, struct Value* out) {
    struct Token* t = start;
    if (t->type == TOK_ID) {
        *out = (Value){.type = VAL_ID, .s = str_alloc(start->val, start->len), .len = start->len};
        t = get_next(t);
    }
    return t;
}

struct Token* parse_str(struct Token* start, struct Value* out) {
    struct Token* t = start;
    if (t->type != TOK_STRING)
        return t;

    *out = (struct Value){.type = VAL_STR, .s = jo_exp(NULL, 1), .len = 0};
    out->s[out->len] = 0;

    t = get_next(t);
    for (; t->type == TOK_FRAG && start->depth + 1 == t->depth; t = get_next(t)) {
        out->s = jo_exp((char*)out->s, out->len + t->len + 1);
        jo_cpy(out->s + out->len, t->val, t->len);
        out->len += t->len;
        out->s[out->len] = 0;
    }
    return t;
}

struct Token* parse_any(struct Token* tok, struct Value* out);

struct Token* parse_kv(struct Token* start, struct Value* k, struct Value* v) {
    struct Token* t = start;
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

struct Token* parse_arr(struct Token* start, struct Value* out) {
    struct Token* t = start;
    int cap = out->len;
    if (t->type == TOK_TERM)
        t = get_next(t);
    for (;;) {
        t = skip_empty(t);

        struct Value el = {};
        struct Token* n = parse_any(t, &el);
        if (n == t || n == NULL)
            break;
        else
            t = n;

        if (cap == out->len) {
            cap = cap * 2 + 2;
            out->p = value_exp(out->p, cap);
        }
        out->p[out->len] = el;
        out->len++;

        // t = skip_empty(t);
        // printf("%i %i", t->depth, start->depth);
        // print_token(t);

        if (t->type == TOK_TERM && t->depth == start->depth)
            t = get_next(t);
        else
            break;
    }
    return t;
}

struct Token* parse_arr_kv(struct Token* start, struct Value* out) {
    struct Token* t = start;
    int cap = out->len;

    if (t->type == TOK_TERM)
        t = get_next(t);
    for (;;) {
        t = skip_empty(t);

        struct Value k = {}, v = {};
        struct Token* n = parse_kv(t, &k, &v);
        if (t == n || n == NULL)
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
            t = get_next(t);
        else
            break;
    }
    return t;
}

int is_kv(struct Token* tok) {
    if (tok->type == TOK_ID) {
        struct Token* t = get_next(tok);
        if (t->type != TOK_TERM && t->type != TOK_NONE && t->depth == tok->depth)
            return 1;
    }
    return 0;
}

struct Token* parse_root(struct Token* start, struct Value* out) {
    struct Token* t = start;
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

struct Token* parse_group(struct Token* start, struct Value* out) {
    struct Token* t = start;
    if (t->type == TOK_GROUP) {
        t = get_next(t);
        t = parse_root(t, out);
    }
    return t;
}

struct Token* parse_any(struct Token* tok, struct Value* out) {
    switch (tok->type) {
        case TOK_NUM:
        case TOK_OP:     return parse_num(tok, out);
        case TOK_DOT:    return parse_dec(tok, out);
        case TOK_ID:     return parse_id(tok, out);
        case TOK_STRING: return parse_str(tok, out);
        case TOK_GROUP:  return parse_group(tok, out);
        default:         return NULL;
    }
}

const char* tokentype_name(enum TokenType type) {
    switch (type) {
        case TOK_NONE:    return "NONE";
        case TOK_ID:      return "ID";
        case TOK_NUM:     return "NUM";
        case TOK_SYM:     return "SYM";
        case TOK_SEP:     return "SEP";
        case TOK_GROUP:   return "GROUP";
        case TOK_STRING:  return "STRING";
        case TOK_COMMENT: return "COMMENT";
        case TOK_FRAG:    return "FRAG";
        case TOK_TERM:    return "TERM";
        case TOK_DOT:     return "DOT";
        default:          return "WTF";
    }
}

void token_print(struct Token* t) {
    if (!t->type || t->type == TOK_EMPTY)
        return;

    char buf[256];
    int p = 0;

    const char* name = tokentype_name(t->type);
    for (int i = 0; i < t->depth; i++)
        p += jo_fmt(buf + p, "| ");

    p += jo_fmt(buf + p, "%-3s @%d:%d", name, t->line, t->col);

    if (t->type == TOK_SYM && t->data)
        p += jo_fmt(buf + p, " (%i)", t->data);

    p += jo_fmt(buf + p, "\"");
    if ((t->type == TOK_TERM && t->data == '\n'))
        p += jo_fmt(buf + p, "n");
    else
        p += jo_fmt(buf + p, "%.*s", t->len, t->val);
    p += jo_fmt(buf + p, "\"\n");
    jo_echo(buf, p);
}

#ifdef JO_USE_LINUX
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
void* jo_new(unsigned long size) { return malloc(size); }
void jo_del(void* ptr) { free(ptr); }
void* jo_exp(void* ptr, unsigned long size) { return realloc(ptr, size); }
void jo_echo(const char* s, size_t len) { fwrite(s, 1, len, stdout); }
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
