#pragma once
#include <stddef.h>

int fs_format(const char *fs_filename, size_t size);
int fs_init(const char *fs_filename);
void fs_close();
int fs_mkdir(const char *name);
int fs_ls();
int fs_touch(const char *name);
int fs_append(const char *filename, const char *text);