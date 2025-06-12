#include "filesystem.h"
#include "block_manager.h"
#include "data_structures.h"
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>

static superblock_t *g_superblock = NULL;
static uint32_t* g_fat = NULL;
static uint8_t* g_bitmap = NULL;
static fcb_t* g_fcbs = NULL;

int fs_format(const char *fs_filename, size_t size) {
    int res = bm_init(fs_filename, size);
    if (res != 0) {
        return res; // Return error code from block manager
    }
    uint32_t total_blocks = size / BLOCK_SIZE;

    //calculate layout
    uint32_t superblock_block = 0;
    uint32_t bitmap_blocks = (total_blocks + 7) / 8 / BLOCK_SIZE + 1;
    // FAT size in blocks
    uint32_t fat_blocks = (total_blocks * sizeof(uint32_t)) / BLOCK_SIZE + 1;
    // FCBs size in blocks
    uint32_t fcb_blocks = (MAX_FCB_COUNT * sizeof(fcb_t)) / BLOCK_SIZE + 1;
    // Data blocks start after bitmap, FAT, and FCBs
    uint32_t data_start_block = 1 + bitmap_blocks + fat_blocks + fcb_blocks;

    g_superblock = (superblock_t*)bm_get_block_address(superblock_block);
    g_superblock->total_blocks = total_blocks;
    g_superblock->free_blocks = total_blocks - data_start_block - 1;
    g_superblock->bitmap_start_block = superblock_block + 1; // Bitmap starts after the superblock
    g_superblock->fat_start_block = g_superblock->bitmap_start_block + bitmap_blocks; // FAT starts after the bitmap
    g_superblock->data_start_block = g_superblock->fat_start_block + fat_blocks; // Data blocks start after the FAT
    g_superblock->root_dir_fcb_index = 0; // Root directory FCB index starts at 0

    // Initialize bitmap
    g_bitmap = (uint8_t*)bm_get_block_address(g_superblock->bitmap_start_block);
    memset(g_bitmap, 0, bitmap_blocks*BLOCK_SIZE);
   
    //FAT
    g_fat = (uint32_t*)bm_get_block_address(g_superblock->fat_start_block);
    for(uint32_t i = 0; i < total_blocks; i++) {
        g_fat[i] = FAT_FREE; // Initialize all blocks as free
    }
    
    //FCB ARRAY
    g_fcbs = (fcb_t*)bm_get_block_address(g_superblock->fat_start_block + fat_blocks);
    memset(g_fcbs, 0, MAX_FCB_COUNT*sizeof(fcb_t)); // Initialize FCBs to zero

    // Set the root directory FCB
    g_fcbs[0].type = FILE_TYPE_DIRECTORY;
    g_fcbs[0].size = 0;
    g_fcbs[0].first_block_index = g_superblock->data_start_block; // Root directory starts at data start block

    // Mark the first block of the root directory as used in the FAT
    g_fat[g_fcbs[0].first_block_index] = FAT_EOF;

    //update the bitmap
    g_bitmap[g_fcbs[0].first_block_index / 8] |= (1 << (g_fcbs[0].first_block_index % 8));
    return 0;
}

int fs_init(const char *fs_filename) {
    
    struct stat st;
    if(stat(fs_filename, &st) != 0) {
        perror("Failed to stat filesystem file");
        return -1; // File does not exist
    }
    if(bm_init(fs_filename, st.st_size) != 0) {
        perror("Failed to initialize block manager");
        return -2; // Block manager initialization failed
    }
    //superblock pointer
    g_superblock = (superblock_t*)bm_get_block_address(0);
    
    //main areas pointers
    g_bitmap = (uint8_t*)bm_get_block_address(g_superblock->bitmap_start_block);
    g_fat = (uint32_t*)bm_get_block_address(g_superblock->fat_start_block);
    uint32_t fat_blocks = (g_superblock->data_start_block - g_superblock->fat_start_block);
    g_fcbs = (fcb_t*)bm_get_block_address(g_superblock->fat_start_block + fat_blocks);
    
    return 0;
}

void fs_close() {
    bm_close();
    
}