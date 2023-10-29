#define _XOPEN_SOURCE 500   // for nftw()

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ftw.h>
#include <sys/param.h>      // for MIN()
#include <getopt.h>

#define MAX_INDENT_LEVEL 128

void helpversion(int argc, char *argv[]);

void walk_dir(const char *dir);

unsigned char *search_bytes;
int search_bytes_size;


int main(int argc, char *argv[]) {

    if (argc > 3) {
        printf("Usage: %s <dir> <search_bytes>\n\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    if (argc == 2){
    	helpversion(argc, argv);
    	}

    if (argc == 3){
    char *search_bytes_str = argv[2];
    	if (strncmp(search_bytes_str, "0x", 2) != 0) {
        printf("Search bytes must start with '0x' prefix.\n");
        exit(EXIT_FAILURE);
    }
    search_bytes_str += 2; // ok

    // Parse search bytes argument and create dynamic array
    search_bytes_size = strlen(search_bytes_str) / 2;
    search_bytes = malloc(search_bytes_size);
    for (int i = 0; i < search_bytes_size; i++) {
        sscanf(search_bytes_str + i * 2, "%2hhx", &search_bytes[i]);
    }

    walk_dir(argv[1]);

    free(search_bytes);

    return EXIT_SUCCESS;
    }
}

void helpversion(int argc, char *argv[]){
    static struct option long_options[] = {
        {"help", no_argument, 0, 0},
        {"version", no_argument, 0, 1}};
    int option_index = 0;
    int choice;
    while ((choice = getopt_long(argc, argv, "vh", long_options, &option_index)) != -1)
    {
        switch (choice)
        {
        case 0://if choice 0 or h - print help

        case 'h':
            printf("Usage: %s <dir> <string>\n", argv[0]);
            printf("<dir> - directory to search\n");
            printf("<string> - string to search\n");
            exit(EXIT_SUCCESS);
        case 1:

        case 'v'://if choice 1 or v - print version
            printf("Smaglyuk Vladislav\nN32451\nnftw sort\nVersion 0.1\n");
            exit(EXIT_SUCCESS);
        case '?':
            exit(EXIT_FAILURE);
        }
    }
}

void print_entry(int level, int type, const char *path, const struct stat *sb) {
    if (getenv("LAB11DEBUG") != NULL) {
        fprintf(stderr, "Debug mode is enabled.\n");
    }

    if (!strcmp(path, ".") || !strcmp(path, ".."))
        return;

    char indent[MAX_INDENT_LEVEL] = {0};
    memset(indent, ' ', MIN((size_t)level, MAX_INDENT_LEVEL));

    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        fprintf(stderr, "Failed to open file: %s\n", path);
        return;
    }

    unsigned char *buf = malloc(search_bytes_size);
    int offset = 0;
    int byte;
    while ((byte = fgetc(file)) != EOF) {
        buf[offset] = byte;
        offset++;

        if (offset == search_bytes_size) {
            if (memcmp(buf, search_bytes, search_bytes_size) == 0) {
                printf("%sFound bytes ", indent);
                for (int i = 0; i < search_bytes_size; i++) {
                    printf("0x%02X ", search_bytes[i]);
                }
                printf("at offset %ld in file: %s\n", ftell(file) - search_bytes_size, path);
                printf("%s[%d] %s (size: %ld)\n", indent, type, path, sb->st_size);
            }

            offset = 0;
        }
    }

    free(buf);
    fclose(file);
} 

int walk_func(const char *fpath,const struct stat *sb, 
        int typeflag, struct FTW *ftwbuf) {
    
    print_entry(ftwbuf->level, typeflag, fpath, sb); 
   
    return 0;
}

void walk_dir(const char *dir) {
    int res = nftw(dir, walk_func, 10, FTW_PHYS);
        if (getenv("LAB11DEBUG") != NULL) {
    	fprintf(stderr, "Debug mode is enabled.\n");
    }
    if (res < 0) {
        fprintf(stderr, "ntfw() failed: %s\n", strerror(errno));
    }
}
