#pragma once

__attribute__((visibility("default"))) void external_api(void *src, size_t n);

void external_private_impl(void);