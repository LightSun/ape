#ifndef global_store_h
#define global_store_h

#ifndef APE_AMALGAMATED
#include "common.h"
#include "collections.h"
#include "object.h"
#endif

typedef struct symbol symbol_t;
typedef struct gcmem gcmem_t;
typedef struct global_store global_store_t;

APE_INTERNAL global_store_t *global_store_make(allocator_t *alloc, gcmem_t *mem);
APE_INTERNAL void global_store_destroy(global_store_t *store);
APE_INTERNAL const symbol_t* global_store_get_symbol(global_store_t *store, const char *name);
APE_INTERNAL object_t global_store_get_object(global_store_t *store, const char *name);
APE_INTERNAL bool global_store_set(global_store_t *store, const char *name, object_t object);
APE_INTERNAL object_t global_store_get_object_at(global_store_t *store, int ix, bool *out_ok);
APE_INTERNAL bool global_store_set_object_at(global_store_t *store, int ix, object_t object);
APE_INTERNAL object_t *global_store_get_object_data(global_store_t *store);
APE_INTERNAL int global_store_get_object_count(global_store_t *store);

#endif /* global_store_h */
