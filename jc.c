

// enum {
//     tok_none, tok_name, tok_number, tok_string, tok_char,
//     tok_struct, tok_if, tok_else, tok_while, tok_for, tok_return, tok_break, tok_continue,
//     tok_add, tok_sub, tok_mul, tok_div, tok_mod, tok_eq, tok_neq, tok_lt, tok_gt, tok_le, tok_ge,
//     tok_and, tok_or, tok_xor, tok_not, tok_lsh, tok_rsh,
//     tok_assign, tok_comma, tok_semicolon, tok_colon, tok_dot,
//     tok_open_paren, tok_close_paren, tok_open_brace, tok_close_brace,
// };
// struct Token {};

// typedef struct Token Token;

// static int jc_lex(const char* src, Token * out, const Token* stack[], int* sp) {
//     int pos = 0;

//     for(;;pos++) {
//         char c = src[pos];
//         const Token* cur = stack[*sp];
//         if(c == 0) return pos;

//         switch (c) {

//         }
//     }
// }


enum OPCODE {
    OP_POP,
    OP_PUSH_CHAR, OP_PUSH_SHORT, OP_PUSH_INT, OP_PUSH_WORD,
    OP_LOAD, //load x from memory
    OP_SAVE,
    OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_CALL, OP_JMP,
    OP_LOAD_CHAR, OP_LOAD_SHORT, OP_LOAD_INT, OP_LOAD_WORD,
    OP_SAVE_CHAR, OP_SAVE_SHORT, OP_SAVE_INT, OP_SAVE_WORD,
    OP_TERM,
};

typedef long long int word;
typedef word (*func)(word);

enum {WORD_SIZE = sizeof(word), SIZE_INT = 4, SIZE_SHORT = 2, SIZE_CHAR = 1};

static inline word exec(unsigned char* code) {
    int i = 0;

    word stack[256];
    static unsigned char mem[1024 * 1024];
    int sp = 0;

    #define CPY(x, size) memcpy(&mem[sp], &x, size); sp += size

    while (1) {
        char c = code[i++];
        switch (c) {
            case OP_POP: sp--; break;
            case OP_PUSH_CHAR: stack[sp++] = *(char*)(&code[i]); i += sizeof(char);  break;
            case OP_PUSH_SHORT: stack[sp++] = *(short*)(&code[i]); i += sizeof(short);  break;
            case OP_PUSH_INT: stack[sp++] = *(int*)(&code[i]); i += sizeof(int);  break;
            case OP_PUSH_WORD: stack[sp++] = *(word*)(&code[i]); i += sizeof(word);  break;
            case OP_ADD: stack[sp-2] += stack[sp-1]; sp--; break;
            case OP_SUB: stack[sp-2] -= stack[sp-1]; sp--; break;
            case OP_MUL: stack[sp-2] *= stack[sp-1]; sp--; break;
            case OP_DIV: stack[sp-2] /= stack[sp-1]; sp--; break;
            case OP_CALL: stack[sp-3] = ((func)stack[sp-2])(stack[sp-1]); sp -= 2; break;
            case OP_JMP: i = stack[sp-1]; sp--; break;
            case OP_LOAD: stack[sp-1] = mem[stack[sp-1]]; break;
            case OP_SAVE: mem[stack[sp-1]] = stack[sp-2]; sp -= 1; break;
            case OP_TERM: goto TERM; break;
        }
    }
    

    TERM:
    return stack[0];
}

#include <sys/mman.h>
#include <stdint.h>

static inline func jet(unsigned char* code) {
    int i = 0;

    i++; // OP_PUSH_INT
    int a = 123;
    i += 4;

    i++; // OP_PUSH_INT
    int b = 2;
    i += 4;

    unsigned char* mem = mmap(
        0, 32,
        PROT_READ | PROT_WRITE | PROT_EXEC,
        MAP_PRIVATE | MAP_ANONYMOUS,
        -1, 0
    );

    unsigned char* p = mem;

    // mov rax, imm64
    *p++ = 0x48;
    *p++ = 0xB8;
    *(int64_t*)p = (int64_t)a;
    p += 8;

    // add rax, imm32
    *p++ = 0x48;
    *p++ = 0x05;
    *(int32_t*)p = (int32_t)b;
    p += 4;

    // ret
    *p++ = 0xC3;

    return (func)mem;
}

#define FUNCOF(f) \
    (unsigned char)((word)(f) >> 0),  \
    (unsigned char)((word)(f) >> 8),  \
    (unsigned char)((word)(f) >> 16), \
    (unsigned char)((word)(f) >> 24), \
    (unsigned char)((word)(f) >> 32), \
    (unsigned char)((word)(f) >> 40), \
    (unsigned char)((word)(f) >> 48), \
    (unsigned char)((word)(f) >> 56)

#define U8(x) ((unsigned char)((x) & 0xFF))

#define INT(v)                    \
    U8((int)(v) >> 0),          \
    U8((int)(v) >> 8),          \
    U8((int)(v) >> 16),         \
    U8((int)(v) >> 24)

#define WORD(v)                    \
    U8((word)(v) >> 0),          \
    U8((word)(v) >> 8),          \
    U8((word)(v) >> 16),         \
    U8((word)(v) >> 24),         \
    U8((word)(v) >> 32),         \
    U8((word)(v) >> 40),         \
    U8((word)(v) >> 48),         \
    U8((word)(v) >> 56)


#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdio.h>

static inline long long ns_now() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}
word hello_world(word w){
    // printf("hello world %lld\n", w);
    return w + 20;
}

void exec2() {
        int a = 3;
        int b = 2;
        int c = a + b;
}

int main() {
    unsigned char code[] = {
        OP_PUSH_INT, INT(3),
        OP_PUSH_INT, INT(2),
        OP_ADD,

        OP_TERM
    };

    const int N = 1000 * 1000;


    long long t0 = ns_now();
    func f = jet(0);
    for (int i = 0; i < N; i++) {
        f(0); //630M
        // exec(code); //100M iter
        // exec2(); 1B iter
    }
    long long t1 = ns_now();

    double secs = (t1 - t0) / 1e9;
    printf("exec/sec = %.2f M\n", (N / secs) / 1e6);
    

    word result = exec(code);
    printf("result = %lld\n", result);
    return 0;
}