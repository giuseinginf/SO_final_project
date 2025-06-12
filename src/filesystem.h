#pragma once
#include <stddef.h>

int fs_format(const char *fs_filename, size_t size);
int fs_init(const char *fs_filename);
void fs_close();