#include <stddef.h>
#include <stdint.h>

void *jo_new(size_t size);             // heap alloc
void jo_del(void *ptr);                // heap free
void *jo_mov(void **ptr);              // heap move, invalidate ptr
void *jo_exp(void **ptr, size_t size); // mov + heap expand/realloc

#define JO_BUN_DEF(type)                                                       \
  struct _jo_bun_##type {                                                      \
    type *ptr;                                                                 \
    uint32_t len;                                                              \
    uint32_t flag;                                                             \
  };

#define jo_bun(type) struct _jo_bun_##type
#define jo_bun_size(val) (sizeof(*(val).ptr))

enum JO_STR_FLAG { JO_STR_NULLT = 1 };

struct jo_str {
  char *ptr;
  uint32_t len;
  uint32_t flag;
};

typedef struct jo_str jo_str;

jo_str jo_str_lit(const char *str); // from literal
jo_str jo_str_mov(jo_str *str);
jo_str jo_str_ref(jo_str str);
jo_str jo_str_cpy(jo_str str);

/// stringify

size_t jo_stry_int(long i, char *buf);
size_t jo_stry_float(double f, char *buf);
size_t jo_stry_bool(int b, char *buf);