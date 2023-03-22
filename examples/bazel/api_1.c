#include "api_1.h"

#include <stdio.h>

#include "external_dep.h"

void my_api(void) {
  puts("my_api called, calling external dep");
  char buf[0x41];
  external_api(buf, sizeof(buf));
}

void internal_impl(void) {
  puts("internal implementation, calling `my_api`");
  my_api();
}