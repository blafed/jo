#include "jo.h"
#include <stdlib.h>
#include <stdio.h>

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

void jo_echo(const char* fmt, size_t len) {
    printf("%.*s", (int)len, fmt);
}


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

size_t jo_stry_int(long i, char *buf) {
    int n = snprintf(buf, 32, "%ld", i);
    return n > 0 ? (size_t)n : 0;
}

size_t jo_stry_float(double f, char *buf) {
    int n = snprintf(buf, 64, "%.17g", f);
    return n > 0 ? (size_t)n : 0;
}

size_t jo_stry_bool(int b, char *buf) {
    if (b) {
        buf[0] = 't'; buf[1] = 'r'; buf[2] = 'u'; buf[3] = 'e';
        return 4;
    } else {
        buf[0] = 'f'; buf[1] = 'a'; buf[2] = 'l'; buf[3] = 's'; buf[4] = 'e';
        return 5;
    }
}

JO_DEF_DEL(int) {
  return;
}

int main (){
  jo_bun(int) intArr = jo_bun_new(int, 100);
  jo_bun_del(int, intArr);
  

  return 0;
}