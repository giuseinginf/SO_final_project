#include <stdio.h>
#include <stdlib.h>
#include "filesystem.h"

#define FS_FILENAME "myfilesystem.disk"
#define FS_SIZE (4096 * 128) // 512 kib

int main() {
    // Format the filesystem
    printf("Formatting filesystem '%s' with size %d bytes...\n", FS_FILENAME, FS_SIZE);
    if(fs_init(FS_FILENAME) != 0) {
        printf("Failed to initialize filesystem. Formatting...\n");
        if(fs_format(FS_FILENAME, FS_SIZE) != 0) {
            printf("Failed to format filesystem.\n");
            return 1;
        }
    }else{
        printf("Filesystem initialized successfully.\n");
    }

    fs_close();
    printf("Filesystem operations completed successfully.\n");
    return 0;
}