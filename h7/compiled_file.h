#ifndef compiled_file_h
#define compiled_file_h

#ifndef APE_AMALGAMATED
#include "common.h"
#include "collections.h"
#endif

typedef struct compiled_file compiled_file_t;

typedef struct compiled_file {
    allocator_t *alloc;
    char *dir_path;
    char *path;
    ptrarray(char*) *lines;
} compiled_file_t;

APE_INTERNAL compiled_file_t* compiled_file_make(allocator_t *alloc, const char *path);
APE_INTERNAL void compiled_file_destroy(compiled_file_t *file);

#endif /* compiled_file_h */
