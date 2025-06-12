#pragma once
#include <stdint.h>
#include <stddef.h>

int bm_init(const char *filename, size_t size);
void bm_close();
void* bm_get_block_address(uint32_t block_number);