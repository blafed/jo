#define JO_USE_LINUX
#define JO_USE_IMPL
#include "jo.h"

#include <stdlib.h>
#include <stdio.h>

size_t file(const char* path, char** out) {
    FILE* f = fopen(path, "rb");
    if (!f) abort();
    fseek(f, 0, SEEK_END);
    size_t fsize = ftell(f);
    if (!fsize) abort();
    fseek(f, 0, SEEK_SET);
    *out = malloc(fsize + 1);
    fread(*out, 1, fsize, f);
    fclose(f);
    (*out)[fsize] = 0;
    return fsize;
}

Value jo(const char* src);

void test_api() {
    Value obj = jo("{x 'hi there', y 1}");
    Value val = value_get_key(obj, "y");
    Value another = value_obj();

    int cmp = value_cmp(val, VALUE_TRUE);
    if (!cmp)
        printf("all equal\n");

    char ser[256];
    int size = serialize(obj, ser);
    fwrite(ser, 1, size, stdout);
    printf("%i \n", val.type);
}

int main() {
    // test_api();
    // return 0;
    char* src;
    size_t fsize = file("ex/0.2.jo", &src);

    static Token tokens[1000];
    static Token* stack[256];

    int len = tokenize(src, tokens);
    Token* err = organize(tokens, stack);
    for (int i = 0; i < len; i++)
        token_print(&tokens[i]);

    for (int i = 0; i < len; i++)
        token_print(&tokens[i]);

    if (err) {
        printf("unexpected token: ");
        token_print(err);
        return 0;
    }

    Value obj = {};
    Token* end = parse(tokens, &obj);
    int parsed = end - tokens;

    if (parsed != len) {
        printf("parser error (%d < %d) at ", parsed, len);
        token_print(&tokens[parsed]);
    }
    char* ser = malloc(20000);
    int size = serialize(obj, ser);
    fwrite(ser, 1, size, stdout);
    printf("\n");

    return 0;
}

Value jo(const char* src) {
    static Token tokens[10000];
    static Token* stack[256];

    Value val = {};

    tokenize(src, tokens);
    organize(tokens, stack);
    parse(tokens, &val);

    if (val.type == VAL_ARR && val.len == 1)
        return val.p[0];

    return val;
}