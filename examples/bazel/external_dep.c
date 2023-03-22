#include <stdio.h>
#include <string.h>

__attribute__((visibility("default"))) void external_api(void *src,
                                                         const size_t n) {
  puts("external_api called, calling external private impl");
  memset(src, 0, n);
}

void external_private_impl(void) {
  puts("external private implementation called");
}