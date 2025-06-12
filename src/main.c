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
        fs_init(FS_FILENAME); // Reinitialize after formatting
    }

    printf("Creating test_dir directory...\n");
    if(fs_mkdir("test_dir") == 0)
        printf("Directory 'test_dir' created successfully.\n");
    else
        printf("Failed to create directory 'test_dir'.\n");

    fs_touch("file1.txt"); // Create a file
    fs_append("file1.txt", "Hello"); // Append text to the file
    fs_append("file1.txt", " World!"); // Append more text
    fs_ls(); // List the contents of the filesystem
    fs_close();
    printf("Filesystem operations completed successfully.\n");
    return 0;
}