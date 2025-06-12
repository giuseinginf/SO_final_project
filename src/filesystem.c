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
    printf("[Debug fs_format]: g_fcbs start at block: %u\n", g_superblock->fat_start_block + fat_blocks);
    memset(g_fcbs, 0, MAX_FCB_COUNT*sizeof(fcb_t)); // Initialize FCBs to zero
    for(int i = 0; i < 10; ++i){
        printf("[Debug fs_format]: FCB %d: Type: %d, Size: %u, First Block Index: %u\n", i, g_fcbs[i].type, g_fcbs[i].size, g_fcbs[i].first_block_index);
    }

    for(int i = 0; i < MAX_FCB_COUNT; ++i) {
        g_fcbs[i].type = FILE_TYPE_UNUSED; // Mark all FCBs as unused
    }

    // Set the root directory FCB
    g_fcbs[0].type = FILE_TYPE_DIRECTORY;
    g_fcbs[0].size = 0;
    g_fcbs[0].first_block_index = g_superblock->data_start_block; // Root directory starts at data start block

    // Mark the first block of the root directory as used in the FAT
    g_fat[g_fcbs[0].first_block_index] = FAT_EOF;

    //initializing entries to 0
    directory_entry_t* root_entries = (directory_entry_t*)bm_get_block_address(g_fcbs[0].first_block_index);
    memset(root_entries, 0, BLOCK_SIZE); // Initialize root directory entries to zero

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

static uint32_t allocate_free_block() {
    for (uint32_t i =g_superblock->data_start_block; i < g_superblock->total_blocks; i++) {
        uint8_t byte = g_bitmap[i / 8];
        if(!(byte & (1 << (i % 8)))) {
            // Block is free
            g_bitmap[i / 8] |= (1 << (i % 8)); // Mark block as used in bitmap
            g_fat[i] = FAT_EOF; // Mark block as end of file in FAT
            g_superblock->free_blocks--;
            return i; // Return the index of the allocated block
        }
    }
    return FAT_EOF; // No free block found
}

int fs_mkdir(const char *name) {
    if (strlen(name) >= MAX_FILENAME_LEN) {
        fprintf(stderr, "Directory name too long\n");
        return -1; // Directory name exceeds maximum length
    }

    // Find a free FCB
    int free_fcb = -1;
    for (int i = 1; i < MAX_FCB_COUNT; ++i) {
        if (g_fcbs[i].type != FILE_TYPE_DIRECTORY && g_fcbs[i].type != FILE_TYPE_REGULAR) {
            free_fcb = i;
            break;
        }
    }
    if (free_fcb == -1) {
        fprintf(stderr, "No free FCB available\n");
        return -2; // No free FCB available
    }

    uint32_t new_block = allocate_free_block();
    if (new_block == FAT_EOF) {
        fprintf(stderr, "No free blocks available\n");
        return -3; // No free blocks available
    }
    // Initialize the new directory FCB
    fcb_t *fcb = &g_fcbs[free_fcb];
    fcb->type = FILE_TYPE_DIRECTORY;
    fcb->size = 0;
    fcb->first_block_index = new_block;
    
    //add to the root directory
    uint32_t root_block = g_fcbs[0].first_block_index;
    directory_entry_t* entries = (directory_entry_t*)bm_get_block_address(root_block);

    for(size_t i = 0; i < MAX_DIR_ENTRIES_PER_BLOCK; ++i) {
        if (entries[i].fcb_index == 0 && entries[i].name[0] == '\0') { // Find an empty entry
            strncpy(entries[i].name, name, MAX_FILENAME_LEN -1);
            entries[i].name[MAX_FILENAME_LEN - 1] = '\0'; // Ensure null termination
            entries[i].fcb_index = free_fcb; // Link to the new FCB
            
            fcb->size += sizeof(directory_entry_t); // Update size of the directory
            return 0; // Success
        }
    }
    fprintf(stderr, "Root directory is full, cannot add new directory\n");
    return -4; // Root directory is full
}

int fs_ls() {
    fcb_t *root_fcb = &g_fcbs[g_superblock->root_dir_fcb_index];
    uint32_t root_block = root_fcb->first_block_index;
    directory_entry_t* entries = (directory_entry_t*)bm_get_block_address(root_block);
    
    printf("Contents of root directory:\n");
    for(size_t i = 0; i < MAX_DIR_ENTRIES_PER_BLOCK; ++i) {
    directory_entry_t* entry = &entries[i];
    if(entry->fcb_index >= MAX_FCB_COUNT){
        fprintf(stderr, "Invalid FCB index in directory entry\n");
        continue; // Skip invalid entries
    }    
    fcb_t *fcb = &g_fcbs[entry->fcb_index];
    if(fcb->type != FILE_TYPE_DIRECTORY && fcb->type != FILE_TYPE_REGULAR) {
        fprintf(stderr, "Invalid FCB type for entry '%s'\n", entry->name);
        continue; // Skip invalid entries
    }
    const char *type_str = (fcb->type == FILE_TYPE_DIRECTORY) ? "Directory" : "File";
    printf("Name: %s, Type: %s, Size: %u bytes\n", entry->name, type_str, fcb->size);
    }
    return 0;
}

int fs_touch(const char *name) {
    if (strlen(name) >= MAX_FILENAME_LEN) {
        fprintf(stderr, "File name too long\n");
        return -1; // File name exceeds maximum length
    }
    // Find a free FCB
    int free_fcb = -1;
    for (int i = 1; i < MAX_FCB_COUNT; ++i) {
        if (g_fcbs[i].type == FILE_TYPE_UNUSED) {
            free_fcb = i;
            break;
        }
    }
if(free_fcb == -1){
    fprintf(stderr, "No free FCB available\n");
    return -2; // No free FCB available
}

//no need for block for empty file, just allocate a block
uint32_t data_block = allocate_free_block();
if (data_block == FAT_EOF) {
    fprintf(stderr, "No free blocks available\n");
    return -3; // No free blocks available
}
// Initialize the new file FCB
fcb_t *fcb = &g_fcbs[free_fcb];
fcb->type = FILE_TYPE_REGULAR;
fcb->size = 0; // Initially, the file size is 0
fcb->first_block_index = data_block;
//add to the root directory
uint32_t root_block = g_fcbs[0].first_block_index;
directory_entry_t* entries = (directory_entry_t*)bm_get_block_address(root_block);

for(size_t i = 0; i < MAX_DIR_ENTRIES_PER_BLOCK; ++i) {
    if (entries[i].fcb_index == 0 && entries[i].name[0] == '\0') { // Find an empty entry
        strncpy(entries[i].name, name, MAX_FILENAME_LEN -1);
        entries[i].name[MAX_FILENAME_LEN - 1] = '\0'; // Ensure null termination
        entries[i].fcb_index = free_fcb; // Link to the new FCB
        
        fcb->size += sizeof(directory_entry_t); // Update size of the directory
        return 0; // Success
    }
}
fprintf(stderr, "Root directory is full, cannot add new file\n");
return -4; // Root directory is full
}

int fs_append(const char *filename, const char *text) {
    uint32_t root_block = g_fcbs[0].first_block_index;
    directory_entry_t* entries = (directory_entry_t*)bm_get_block_address(root_block);
    int found = -1;
    // Find the file in the root directory
    for(size_t i = 0; i < MAX_DIR_ENTRIES_PER_BLOCK; ++i) {
        if(strncmp(entries[i].name, filename, MAX_FILENAME_LEN) == 0) {
            found = entries[i].fcb_index;
            break;
        }
    }
    if(found == -1) {
        fprintf(stderr, "File '%s' not found\n", filename);
        return -1; // File not found
    }
    fcb_t *fcb = &g_fcbs[found];
    if(fcb->type != FILE_TYPE_REGULAR) {
        fprintf(stderr, "Cannot append to a directory\n");
        return -2; // Not a regular file
    }
    size_t text_length = strlen(text);
    size_t bytes_written = 0;
    uint32_t current_block = fcb->first_block_index;
    size_t file_size = fcb->size;
    // Find the last block of the file
    while (g_fat[current_block] != FAT_EOF){
        current_block = g_fat[current_block];
    }
    //calculate offset in the last block
    size_t offset = file_size % BLOCK_SIZE;
    char* block_data = (char*)bm_get_block_address(current_block);
    //write in the last block as much as there is space
    while (bytes_written < text_length) {
        size_t to_write = BLOCK_SIZE - offset; // Space left in the current block
        if(to_write > text_length - bytes_written) {
            to_write = text_length - bytes_written; // Write only the remaining text
        }
        memcpy(block_data + offset, text + bytes_written, to_write);
        bytes_written += to_write;
        fcb->size += to_write; // Update file size
        
        //if we're not done writing, allocate a new block
        if (bytes_written < text_length) {
            uint32_t new_block = allocate_free_block();
            if (new_block == FAT_EOF) {
                fprintf(stderr, "No free blocks available for appending\n");
                return -3; // No free blocks available
            }
            g_fat[current_block] = new_block; // Link the last block to the new block
            g_fat[new_block] = FAT_EOF; // Mark the new block as end of file
            current_block = new_block; // Move to the new block
            block_data = (char*)bm_get_block_address(current_block); // Get the address of the new block
            offset = 0; // Reset offset for the new block
        } 
    }
    return 0; // Success
}

void fs_close() {
    bm_close();
 }