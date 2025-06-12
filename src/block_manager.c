#include "block_manager.h"
#include "data_structures.h"
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/mman.h>

#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/stat.h>

void* g_disk_memory = NULL;
size_t g_disk_size = 0;
int g_disk_fd = -1;

int bm_init(const char *filename, size_t size) {
    g_disk_fd = open(filename, O_RDWR | O_CREAT, 0666);
    if(g_disk_fd < 0) {
        perror("Failed to open disk file");
        return -1;
    }
    if(ftruncate(g_disk_fd, size) != 0) {
        perror("Failed to set disk size");
        close(g_disk_fd);
        return -2;
    }
    g_disk_memory = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, g_disk_fd, 0);
    if(g_disk_memory == MAP_FAILED) {
        perror("Failed to map disk file to memory");
        close(g_disk_fd);
        return -3;
    }
    g_disk_size = size;
    return 0;
}

void bm_close() {
    if(g_disk_memory && g_disk_memory != MAP_FAILED) {
        munmap(g_disk_memory, g_disk_size);
    }
    if(g_disk_fd != -1) {
        close(g_disk_fd);
    }
}

void* bm_get_block_address(uint32_t block_number) {
    return (u_int8_t*)g_disk_memory + (block_number * BLOCK_SIZE);
}