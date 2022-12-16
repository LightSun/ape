#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifndef APE_AMALGAMATED
#include "gc.h"
#include "object.h"
#endif

static object_data_pool_t* get_pool_for_type(gcmem_t *mem, object_type_t type);
static bool can_data_be_put_in_pool(gcmem_t *mem, object_data_t *data);

gcmem_t *gcmem_make(allocator_t *alloc) {
    gcmem_t *mem = allocator_malloc(alloc, sizeof(gcmem_t));
    if (!mem) {
        return NULL;
    }
    memset(mem, 0, sizeof(gcmem_t));
    mem->alloc = alloc;
    mem->objects = ptrarray_make(alloc);
    if (!mem->objects) {
        goto error;
    }
    mem->objects_back = ptrarray_make(alloc);
    if (!mem->objects_back) {
        goto error;
    }
    mem->objects_not_gced = array_make(alloc, object_t);
    if (!mem->objects_not_gced) {
        goto error;
    }
    mem->allocations_since_sweep = 0;
    mem->data_only_pool.count = 0;

    for (int i = 0; i < GCMEM_POOLS_NUM; i++) {
        object_data_pool_t *pool = &mem->pools[i];
        mem->pools[i].count = 0;
        memset(pool, 0, sizeof(object_data_pool_t));
    }

    return mem;
error:
    gcmem_destroy(mem);
    return NULL;
}

void gcmem_destroy(gcmem_t *mem) {
    if (!mem) {
        return;
    }

    array_destroy(mem->objects_not_gced);
    ptrarray_destroy(mem->objects_back);

    for (int i = 0; i < ptrarray_count(mem->objects); i++) {
        object_data_t *obj = ptrarray_get(mem->objects, i);
        object_data_deinit(obj);
        allocator_free(mem->alloc, obj);
    }
    ptrarray_destroy(mem->objects);

    for (int i = 0; i < GCMEM_POOLS_NUM; i++) {
        object_data_pool_t *pool = &mem->pools[i];
        for (int j = 0; j < pool->count; j++) {
            object_data_t *data = pool->data[j];
            object_data_deinit(data);
            allocator_free(mem->alloc, data);
        }
        memset(pool, 0, sizeof(object_data_pool_t));
    }

    for (int i = 0; i < mem->data_only_pool.count; i++) {
        allocator_free(mem->alloc, mem->data_only_pool.data[i]);
    }

    allocator_free(mem->alloc, mem);
}

object_data_t* gcmem_alloc_object_data(gcmem_t *mem, object_type_t type) {
    object_data_t *data = NULL;
    mem->allocations_since_sweep++;
    if (mem->data_only_pool.count > 0) {
        data = mem->data_only_pool.data[mem->data_only_pool.count - 1];
        mem->data_only_pool.count--;
    } else {
        data = allocator_malloc(mem->alloc, sizeof(object_data_t));
        if (!data) {
            return NULL;
        }
    }

    memset(data, 0, sizeof(object_data_t));

    APE_ASSERT(ptrarray_count(mem->objects_back) >= ptrarray_count(mem->objects));
    // we want to make sure that appending to objects_back never fails in sweep
    // so this only reserves space there.
    bool ok = ptrarray_add(mem->objects_back, data);
    if (!ok) {
        allocator_free(mem->alloc, data);
        return NULL;
    }
    ok = ptrarray_add(mem->objects, data);
    if (!ok) {
        allocator_free(mem->alloc, data);
        return NULL;
    }
    data->mem = mem;
    data->type = type;
    return data;
}

object_data_t* gcmem_get_object_data_from_pool(gcmem_t *mem, object_type_t type) {
    object_data_pool_t *pool = get_pool_for_type(mem, type);
    if (!pool || pool->count <= 0) {
        return NULL;
    }
    object_data_t *data = pool->data[pool->count - 1];

    APE_ASSERT(ptrarray_count(mem->objects_back) >= ptrarray_count(mem->objects));

    // we want to make sure that appending to objects_back never fails in sweep
    // so this only reserves space there.
    bool ok = ptrarray_add(mem->objects_back, data);
    if (!ok) {
        return NULL;
    }
    ok = ptrarray_add(mem->objects, data);
    if (!ok) {
        return NULL;
    }

    pool->count--;

    return data;
}

void gc_unmark_all(gcmem_t *mem) {
    for (int i = 0; i < ptrarray_count(mem->objects); i++) {
        object_data_t *data = ptrarray_get(mem->objects, i);
        data->gcmark = false;
    }
}

void gc_mark_objects(object_t *objects, int count) {
    for (int i = 0; i < count; i++) {
        object_t obj = objects[i];
        gc_mark_object(obj);
    }
}

void gc_mark_object(object_t obj) {
    if (!object_is_allocated(obj)) {
        return;
    }

    object_data_t *data = object_get_allocated_data(obj);
    if (data->gcmark) {
        return;
    }

    data->gcmark = true;
    switch (data->type) {
        case OBJECT_MAP: {
            int len = object_get_map_length(obj);
            for (int i = 0; i < len; i++) {
                object_t key = object_get_map_key_at(obj, i);
                if (object_is_allocated(key)) {
                    object_data_t *key_data = object_get_allocated_data(key);
                    if (!key_data->gcmark) {
                        gc_mark_object(key);
                    }
                }
                object_t val = object_get_map_value_at(obj, i);
                if (object_is_allocated(val)) {
                    object_data_t *val_data = object_get_allocated_data(val);
                    if (!val_data->gcmark) {
                        gc_mark_object(val);
                    }
                }
            }
            break;
        }
        case OBJECT_ARRAY: {
            int len = object_get_array_length(obj);
            for (int i = 0; i < len; i++) {
                object_t val = object_get_array_value_at(obj, i);
                if (object_is_allocated(val)) {
                    object_data_t *val_data = object_get_allocated_data(val);
                    if (!val_data->gcmark) {
                        gc_mark_object(val);
                    }
                }
            }
            break;
        }
        case OBJECT_FUNCTION: {
            function_t *function = object_get_function(obj);
            for (int i = 0; i < function->free_vals_count; i++) {
                object_t free_val = object_get_function_free_val(obj, i);
                gc_mark_object(free_val);
                if (object_is_allocated(free_val)) {
                    object_data_t *free_val_data = object_get_allocated_data(free_val);
                    if (!free_val_data->gcmark) {
                        gc_mark_object(free_val);
                    }
                }
            }
            break;
        }
        default: {
            break;
        }
    }
}

void gc_sweep(gcmem_t *mem) {
    gc_mark_objects(array_data(mem->objects_not_gced), array_count(mem->objects_not_gced));

    APE_ASSERT(ptrarray_count(mem->objects_back) >= ptrarray_count(mem->objects));

    ptrarray_clear(mem->objects_back);
    for (int i = 0; i < ptrarray_count(mem->objects); i++) {
        object_data_t *data = ptrarray_get(mem->objects, i);
        if (data->gcmark) {
            // this should never fail because objects_back's size should be equal to objects
            bool ok = ptrarray_add(mem->objects_back, data);
            (void)ok;
            APE_ASSERT(ok);
        } else {
            if (can_data_be_put_in_pool(mem, data)) {
                object_data_pool_t *pool = get_pool_for_type(mem, data->type);
                pool->data[pool->count] = data;
                pool->count++;
            } else {
                object_data_deinit(data);
                if (mem->data_only_pool.count < GCMEM_POOL_SIZE) {
                    mem->data_only_pool.data[mem->data_only_pool.count] = data;
                    mem->data_only_pool.count++;
                } else {
                    allocator_free(mem->alloc, data);
                }
            }
        }
    }
    ptrarray(object_t) *objs_temp = mem->objects;
    mem->objects = mem->objects_back;
    mem->objects_back = objs_temp;
    mem->allocations_since_sweep = 0;
}

bool gc_disable_on_object(object_t obj) {
    if (!object_is_allocated(obj)) {
        return false;
    }
    object_data_t *data = object_get_allocated_data(obj);
    if (array_contains(data->mem->objects_not_gced, &obj)) {
        return false;
    }
    bool ok = array_add(data->mem->objects_not_gced, &obj);
    return ok;
}

void gc_enable_on_object(object_t obj) {
    if (!object_is_allocated(obj)) {
        return;
    }
    object_data_t *data = object_get_allocated_data(obj);
    array_remove_item(data->mem->objects_not_gced, &obj);
}

int gc_should_sweep(gcmem_t *mem) {
    return mem->allocations_since_sweep > GCMEM_SWEEP_INTERVAL;
}

// INTERNAL
static object_data_pool_t* get_pool_for_type(gcmem_t *mem, object_type_t type) {
    switch (type) {
        case OBJECT_ARRAY:  return &mem->pools[0];
        case OBJECT_MAP:    return &mem->pools[1];
        case OBJECT_STRING: return &mem->pools[2];
        default:            return NULL;
    }
}

static bool can_data_be_put_in_pool(gcmem_t *mem, object_data_t *data) {
    object_t obj = object_make_from_data(data->type, data);

    // this is to ensure that large objects won't be kept in pool indefinitely
    switch (data->type) {
        case OBJECT_ARRAY: {
            if (object_get_array_length(obj) > 1024) {
                return false;
            }
            break;
        }
        case OBJECT_MAP: {
            if (object_get_map_length(obj) > 1024) {
                return false;
            }
            break;
        }
        case OBJECT_STRING: {
            if (!data->string.is_allocated || data->string.capacity > 4096) {
                return false;
            }
            break;
        }
        default:
            break;
    }

    object_data_pool_t *pool = get_pool_for_type(mem, data->type);
    if (!pool || pool->count >= GCMEM_POOL_SIZE) {
        return false;
    }

    return true;
}
