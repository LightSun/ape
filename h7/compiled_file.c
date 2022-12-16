#ifndef APE_AMALGAMATED
#include "compiled_file.h"
#endif

compiled_file_t* compiled_file_make(allocator_t *alloc, const char *path) {
    compiled_file_t *file = allocator_malloc(alloc, sizeof(compiled_file_t));
    if (!file) {
        return NULL;
    }
    memset(file, 0, sizeof(compiled_file_t));
    file->alloc = alloc;
    const char *last_slash_pos = strrchr(path, '/');
    if (last_slash_pos) {
        size_t len = last_slash_pos - path + 1;
        file->dir_path = ape_strndup(alloc, path, len);
    } else {
        file->dir_path = ape_strdup(alloc, "");
    }
    if (!file->dir_path) {
        goto error;
    }
    file->path = ape_strdup(alloc, path);
    if (!file->path) {
        goto error;
    }
    file->lines = ptrarray_make(alloc);
    if (!file->lines) {
        goto error;
    }
    return file;
error:
    compiled_file_destroy(file);
    return NULL;
}

void compiled_file_destroy(compiled_file_t *file) {
    if (!file) {
        return;
    }
    for (int i = 0; i < ptrarray_count(file->lines); i++) {
        void *item = ptrarray_get(file->lines, i);
        allocator_free(file->alloc, item);
    }
    ptrarray_destroy(file->lines);
    allocator_free(file->alloc, file->dir_path);
    allocator_free(file->alloc, file->path);
    allocator_free(file->alloc, file);
}
