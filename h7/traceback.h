#ifndef traceback_h
#define traceback_h

#ifndef APE_AMALGAMATED
#include "common.h"
#include "collections.h"
#endif

typedef struct vm vm_t;

typedef struct traceback_item {
    char *function_name;
    src_pos_t pos;
} traceback_item_t;

typedef struct traceback {
    allocator_t *alloc;
    array(traceback_item_t)* items;
} traceback_t;

APE_INTERNAL traceback_t* traceback_make(allocator_t *alloc);
APE_INTERNAL void traceback_destroy(traceback_t *traceback);
APE_INTERNAL bool traceback_append(traceback_t *traceback, const char *function_name, src_pos_t pos);
APE_INTERNAL bool traceback_append_from_vm(traceback_t *traceback, vm_t *vm);
APE_INTERNAL bool traceback_to_string(const traceback_t *traceback, strbuf_t *buf);
APE_INTERNAL const char* traceback_item_get_line(traceback_item_t *item);
APE_INTERNAL const char* traceback_item_get_filepath(traceback_item_t *item);

#endif /* traceback_h */
