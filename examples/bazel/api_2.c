#include "api_2.h"

#include <stdio.h>

#include "api_1.h"

void another_api(void) {
  puts("another_api called, calling my_api");
  my_api();
}