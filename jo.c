#include "jo.h"
#ifdef linux
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


typedef struct {int x;} day;

void hi(day a){
    day day;
    day.x = 1;
}

void *jo_new(size_t size) { return malloc(size); }

void jo_del(void *ptr) { free(ptr); }

void *jo_mov(void **ptr) {
  void *tmp = *ptr;
  *ptr = NULL;
  return tmp;
}

void *jo_exp(void **ptr, size_t size) {
  void *newptr = realloc(*ptr, size);
  *ptr = NULL;
  return newptr;
}

void jo_cpy(void* dst, void* src, size_t len) { memcpy(dst, src, len); }

void jo_echo(const char* fmt, size_t len) {
    printf("%.*s", (int)len, fmt);
}

int jo_stry_int(long i, char *buf) {
    int n = snprintf(buf, 32, "%ld", i);
    return n > 0 ? (size_t)n : 0;
}

int jo_stry_float(double f, char *buf) {
    int n = snprintf(buf, 64, "%.17g", f);
    return n > 0 ? (size_t)n : 0;
}

void jo_abort(const char *msg, size_t len) {
    if (msg && len)
        fwrite(msg, 1, len, stderr);
    fputc('\n', stderr);
    abort(); // raises SIGABRT
}

int jo_stry_bool(int b, char *buf) {
    if (b) {
        buf[0] = 't'; buf[1] = 'r'; buf[2] = 'u'; buf[3] = 'e';
        return 4;
    } else {
        buf[0] = 'f'; buf[1] = 'a'; buf[2] = 'l'; buf[3] = 's'; buf[4] = 'e';
        return 5;
    }
}

int jo_strp_int(char* buf, long* ptr) {
    int s = 0;
    long v = 0;
    int neg = 0;
    int base = 10;
    int state = 0; //0 sign, 1 zero, 2 digit

    for(;;s++) {
        char c = buf[s];

        if(c == 0) break;
        
        if(state == 0) {
            if(c == '-') neg = 1;
            else if (c == '0') state = 1;
            else if(c == '+' || c > '0' && c <= '9') {
                state = 2;
            } else break;

            if(state != 2) continue;
        } 

        if(state == 1){
            switch (c) {
                case 'x': case 'X': base = 16; break;
                case 'b': case 'B': base = 2; break;
                case 'o': case 'O': base = 8; break;
            }
            state = 2;
            continue;
        }

        if(state == 2) {
            if(c == '_') continue;
            
            int d = -1;
            if (c >= '0' && c <= '9') d = c - '0';
            else if (base == 16) {
                char x = c | 32;
                if (x >= 'a' && x <= 'f') d = x - 'a' + 10;
            }
            if (d < 0 || d >= base) break;

            v = v * base + d;
        }
    }

    if(neg) v = -v;
    *ptr = v;
    return s;
}

#else
#error "not implemented"
#endif


jo_str jo_str_lit(const char *ptr, size_t len) {
    return (jo_str){(char*)ptr, len, JO_BUN_NULLT};
}

jo_str jo_str_mov(jo_str *str) {
  jo_str newstr = *str;
  *str = (jo_str){};
  return newstr;
}

jo_str jo_str_ref(jo_str str) {
    return (jo_str){.ptr = str.ptr, .len = str.len, .flag = str.flag & ~JO_BUN_OWN};
}

jo_str jo_str_cpy(jo_str) {
    return (jo_str){};
}



JO_DEF_DEL(int) {
  return;
}

int main (){
    char* str = "0b1000s";
    long l;
    int dst = jo_strp_int(str, &l);
    printf("%i %ld", dst, l);
    printf("%s",str + dst);
  return 0;
}