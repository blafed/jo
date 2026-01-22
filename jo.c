#include "jo.h"
#include <stdlib.h>

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