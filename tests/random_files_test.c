#include <simplefs.h>
#include <stdio.h>
#include <stdlib.h>

int files = 0;

void generate_files(DirectoryHandle* d, char* str, unsigned index, unsigned length) {
    char c;

    if (index < (length - 1)) {
        for (c = 'a'; c <= 'l'; ++c) {
            str[index] = c;
            generate_files(d, str, index + 1, length);
        }
    } else {
        for (c = 'a'; c <= 'z'; ++c) {
            str[index] = c;
            FileHandle* fh = SimpleFS_createFile(d, str);
            if (fh == NULL) {
                continue;
            }
            SimpleFS_closeFile(fh);
            files += 1;
        }
    }
}

int main(int argc, char* argv[]) {

    SimpleFS fs;
    DiskDriver disk;

    DiskDriver_init(&disk, "disk_random_files_test.img", 1024);
    DirectoryHandle* root_dir = SimpleFS_init(&fs, &disk);

    char* str = (char*) calloc(3, sizeof(char));
    generate_files(root_dir, str, 0, 2);
    printf("Created %d files!\n", files);

    char** names = calloc(root_dir->dcb->num_entries, sizeof(char*));
    int ret = SimpleFS_readDir(names, root_dir);
    if (ret == -1) {
        printf("Something went wrong while listing dir files.\n");
        exit(EXIT_FAILURE);
    }

    printf("***** LIST FILES *****\n");
    int i;
    for (i = 0; i < root_dir->dcb->num_entries; i++)
        printf("File/Dir: %s\n", names[i]);
    printf("**********************\n");

    for (i = 0; i < root_dir->dcb->num_entries; i++) 
        free(names[i]);   
    free(names);

    SimpleFS_closeDir(root_dir);
    printf("TEST COMPLETED. SUCCESS.\n");

    free(str);

    return 0;
}
