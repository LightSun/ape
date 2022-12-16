#ifndef APE_AMALGAMATED
#include "global_store.h"

#include "symbol_table.h"
#include "builtins.h"
#endif

typedef struct global_store {
    allocator_t *alloc;
    dict(symbol_t) *symbols;
    array(object_t) *objects;
} global_store_t;

global_store_t *global_store_make(allocator_t *alloc, gcmem_t *mem) {
    global_store_t *store = allocator_malloc(alloc, sizeof(global_store_t));
    if (!store) {
        return NULL;
    }
    memset(store, 0, sizeof(global_store_t));
    store->alloc = alloc;
    store->symbols = dict_make(alloc, symbol_copy, symbol_destroy);
    if (!store->symbols) {
        goto err;
    }
    store->objects = array_make(alloc, object_t);
    if (!store->objects) {
        goto err;
    }

    if (mem) {
        for (int i = 0; i < builtins_count(); i++) {
            const char *name = builtins_get_name(i);
            object_t builtin = object_make_native_function(mem, name, builtins_get_fn(i), NULL, 0);
            if (object_is_null(builtin)) {
                goto err;
            }
            bool ok = global_store_set(store, name, builtin);
            if (!ok) {
                goto err;
            }
        }
    }

    return store;
err:
    global_store_destroy(store);
    return NULL;
}

void global_store_destroy(global_store_t *store) {
    if (!store) {
        return;
    }
    dict_destroy_with_items(store->symbols);
    array_destroy(store->objects);
    allocator_free(store->alloc, store);
}

const symbol_t* global_store_get_symbol(global_store_t *store, const char *name) {
    return dict_get(store->symbols, name);
}

object_t global_store_get_object(global_store_t *store, const char *name) {
    const symbol_t *symbol = global_store_get_symbol(store, name);
    if (!symbol) {
        return object_make_null();
    }
    APE_ASSERT(symbol->type == SYMBOL_APE_GLOBAL);
    object_t *res = array_get(store->objects, symbol->index);
    return *res;
}

bool global_store_set(global_store_t *store, const char *name, object_t object) {
    const symbol_t *existing_symbol = global_store_get_symbol(store, name);
    if (existing_symbol) {
        bool ok = array_set(store->objects, existing_symbol->index, &object);
        return ok;
    }
    int ix = array_count(store->objects);
    bool ok = array_add(store->objects, &object);
    if (!ok) {
        return false;
    }
    symbol_t *symbol = symbol_make(store->alloc, name, SYMBOL_APE_GLOBAL, ix, false);
    if (!symbol) {
        goto err;
    }
    ok = dict_set(store->symbols, name, symbol);
    if (!ok) {
        symbol_destroy(symbol);
        goto err;
    }
    return true;
err:
    array_pop(store->objects, NULL);
    return false;
}

object_t global_store_get_object_at(global_store_t *store, int ix, bool *out_ok) {
    object_t *res = array_get(store->objects, ix);
    if (!res) {
        *out_ok = false;
        return object_make_null();
    }
    *out_ok = true;
    return *res;
}

bool global_store_set_object_at(global_store_t *store, int ix, object_t object) {
    bool ok = array_set(store->objects, ix, &object);
    return ok;
}

object_t *global_store_get_object_data(global_store_t *store) {
    return array_data(store->objects);
}

int global_store_get_object_count(global_store_t *store) {
    return array_count(store->objects);
}
