#ifndef gc_h
#define gc_h

#ifndef APE_AMALGAMATED
#include "common.h"
#include "collections.h"
#include "object.h"
#endif

typedef struct object_data object_data_t;
typedef struct env env_t;

#define GCMEM_POOL_SIZE 2048
#define GCMEM_POOLS_NUM 3
#define GCMEM_SWEEP_INTERVAL 128

typedef struct object_data_pool {
    object_data_t *data[GCMEM_POOL_SIZE];
    int count;
} object_data_pool_t;

typedef struct gcmem {
    allocator_t *alloc;
    int allocations_since_sweep;

    ptrarray(object_data_t) *objects;
    ptrarray(object_data_t) *objects_back;

    array(object_t) *objects_not_gced;

    object_data_pool_t data_only_pool;
    object_data_pool_t pools[GCMEM_POOLS_NUM];
} gcmem_t;

APE_INTERNAL gcmem_t *gcmem_make(allocator_t *alloc);
APE_INTERNAL void gcmem_destroy(gcmem_t *mem);

APE_INTERNAL object_data_t* gcmem_alloc_object_data(gcmem_t *mem, object_type_t type);
APE_INTERNAL object_data_t* gcmem_get_object_data_from_pool(gcmem_t *mem, object_type_t type);

APE_INTERNAL void gc_unmark_all(gcmem_t *mem);
APE_INTERNAL void gc_mark_objects(object_t *objects, int count);
APE_INTERNAL void gc_mark_object(object_t object);
APE_INTERNAL void gc_sweep(gcmem_t *mem);

APE_INTERNAL bool gc_disable_on_object(object_t obj);
APE_INTERNAL void gc_enable_on_object(object_t obj);

APE_INTERNAL int gc_should_sweep(gcmem_t *mem);

#endif /* gc_h */
