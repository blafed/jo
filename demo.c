#define JO_USE_LINUX
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

void file_write(const char* path, const char* data, size_t len) {
    FILE* f = fopen(path, "wb");

    if (!f) {
        perror(path);
        abort();
    }

    size_t written = fwrite(data, 1, len, f);

    fclose(f);

    if (written != len)
        abort();
}

int main() {
    // const char src[] = "foo 'hi there'";

    char* src = "foo";
    size_t fsize = file("ex/0.1.jo", &src);
    // printf("%s %i", src, src[2]);

    static struct Token tokens[10000];
    static struct Token* stack[256];

    int len = tokenize(src, tokens);
    organize(tokens, stack);

    for (int i = 0; i < len; i++)
        token_print(&tokens[i]);

    // return 0;
    // const struct Value* v = parse(tokens, stack2, &len);
    // struct Value obj = {.type = VAL_OBJ, .len = len, .p = v};
    struct Value obj = {};
    struct Token* end = parse_root(tokens, &obj);
    int parsed = end - tokens;

    if (parsed != len) {
        printf("parser error (%d < %d) at ", parsed, len);
        token_print(&tokens[parsed]);
    }

    char* ser = malloc(20000);
    int size = serialize(obj, ser);
    fwrite(ser, 1, size, stdout);
    // file_write("foo_out.jo", ser, size);

    return 0;
}