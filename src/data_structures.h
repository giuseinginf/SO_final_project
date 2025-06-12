#pragma once
#include <stdint.h>

#define BLOCK_SIZE 4096
#define MAX_FILENAME_LEN 28
#define MAX_DIR_ENTRIES_PER_BLOCK (BLOCK_SIZE / sizeof(directory_entry_y))
#define MAX_FCB_COUNT 1024
#define FAT_EOF 0xFFFFFFFF
#define FAT_FREE 0x00000000

typedef enum{
    FILE_TYPE_REGULAR,
    FILE_TYPE_DIRECTORY
} file_type_t;

typedef struct {
    uint32_t total_blocks;
    uint32_t free_blocks;
    uint32_t root_dir_fcb_index;
    uint32_t bitmap_start_block;
    uint32_t fat_start_block;
    uint32_t data_start_block;
}superblock_t;

typedef struct {
    file_type_t type;
    uint32_t size;
    uint32_t first_block_index;
    //creation time
    //last modified time
    //permissions
}fcb_t;

typedef struct {
    char name[MAX_FILENAME_LEN];
    uint32_t fcb_index;
} directory_entry_t;