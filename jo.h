#include <stddef.h>
#include <stdint.h>

void *jo_new(size_t size);             // heap alloc
void jo_del(void *ptr);                // heap free
void *jo_mov(void **ptr);              // heap move, invalidate ptr
void *jo_exp(void **ptr, size_t size); // mov + heap expand/realloc
void jo_cpy(void* dst, void* src, size_t len);
void jo_echo(const char *slug, size_t len); //no format

size_t jo_stry_int(long i, char *buf);
size_t jo_stry_float(double f, char *buf);
size_t jo_stry_bool(int b, char *buf);


enum JO_BUN_FLAG {JO_BUN_OWN = 1, JO_BUN_NULLT = 2};

#define JO_BUN_DEF(type)                                                 \
  struct _jo_bun_##type {                                                      \
    type *ptr;                                                                 \
    uint32_t len;                                                              \
    uint32_t flag;                                                             \
  };

#define jo_bun(type) struct _jo_bun_##type
#define jo_bun_size(val) (sizeof(*(val).ptr))

JO_BUN_DEF(char);
JO_BUN_DEF(int);
JO_BUN_DEF(float);
typedef jo_bun(char) jo_str;

#define jo_bun_mov(type, bun) ((jo_bun(type)){jo_mov((void**)&(bun)->ptr), (bun)->len, (bun)->flag})
#define jo_bun_cpy(type, bun) {void* _jo_cpy = jo_new(jo_bun_size(ptr) * bun.len); jo_cpy(_jo_cpy, bun.ptr, jo_bun_size(ptr) * bun.len); }
#define jo_bun_new(type, len) ((jo_bun(type)){jo_new(sizeof(type) * len), len, 0})
#define jo_bun_del(type, bun) {for(size_t i = 0; i < bun.len; i++) _jo_del_##type(&bun.ptr[i]); jo_del(bun.ptr); }

jo_str jo_str_lit(const char *str, size_t len); // from literal
jo_str jo_str_mov(jo_str *str);
jo_str jo_str_cpy(jo_str str);

// jo_str jo_str_ref(jo_str str);
/// stringify


#define JO_DEF_DEL(type) void _jo_del_##type(type *ptr)