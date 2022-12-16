#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifndef APE_AMALGAMATED
#include "symbol_table.h"
#include "global_store.h"
#endif

static block_scope_t* block_scope_copy(block_scope_t *scope);
static block_scope_t* block_scope_make(allocator_t *alloc, int offset);
static void block_scope_destroy(block_scope_t *scope);
static bool set_symbol(symbol_table_t *table, symbol_t *symbol);
static int next_symbol_index(symbol_table_t *table);
static int count_num_definitions(symbol_table_t *table);

symbol_t *symbol_make(allocator_t *alloc, const char *name, symbol_type_t type, int index, bool assignable) {
    symbol_t *symbol = allocator_malloc(alloc, sizeof(symbol_t));
    if (!symbol) {
        return NULL;
    }
    memset(symbol, 0, sizeof(symbol_t));
    symbol->alloc = alloc;
    symbol->name = ape_strdup(alloc, name);
    if (!symbol->name) {
        allocator_free(alloc, symbol);
        return NULL;
    }
    symbol->type = type;
    symbol->index = index;
    symbol->assignable = assignable;
    return symbol;
}

void symbol_destroy(symbol_t *symbol) {
    if (!symbol) {
        return;
    }
    allocator_free(symbol->alloc, symbol->name);
    allocator_free(symbol->alloc, symbol);
}

symbol_t* symbol_copy(symbol_t *symbol) {
    return symbol_make(symbol->alloc, symbol->name, symbol->type, symbol->index, symbol->assignable);
}

symbol_table_t *symbol_table_make(allocator_t *alloc, symbol_table_t *outer, global_store_t *global_store, int module_global_offset) {
    symbol_table_t *table = allocator_malloc(alloc, sizeof(symbol_table_t));
    if (!table) {
        return NULL;
    }
    memset(table, 0, sizeof(symbol_table_t));
    table->alloc = alloc;
    table->max_num_definitions = 0;
    table->outer = outer;
    table->global_store = global_store;
    table->module_global_offset = module_global_offset;

    table->block_scopes = ptrarray_make(alloc);
    if (!table->block_scopes) {
        goto err;
    }

    table->free_symbols = ptrarray_make(alloc);
    if (!table->free_symbols) {
        goto err;
    }

    table->module_global_symbols = ptrarray_make(alloc);
    if (!table->module_global_symbols) {
        goto err;
    }

    bool ok = symbol_table_push_block_scope(table);
    if (!ok) {
        goto err;
    }

    return table;
err:
    symbol_table_destroy(table);
    return NULL;
}

void symbol_table_destroy(symbol_table_t *table) {
    if (!table) {
        return;
    }

    while (ptrarray_count(table->block_scopes) > 0) {
        symbol_table_pop_block_scope(table);
    }
    ptrarray_destroy(table->block_scopes);
    ptrarray_destroy_with_items(table->module_global_symbols, symbol_destroy);
    ptrarray_destroy_with_items(table->free_symbols, symbol_destroy);
    allocator_t *alloc = table->alloc;
    memset(table, 0, sizeof(symbol_table_t));
    allocator_free(alloc, table);
}

symbol_table_t* symbol_table_copy(symbol_table_t *table) {
    symbol_table_t *copy = allocator_malloc(table->alloc, sizeof(symbol_table_t));
    if (!copy) {
        return NULL;
    }
    memset(copy, 0, sizeof(symbol_table_t));
    copy->alloc = table->alloc;
    copy->outer = table->outer;
    copy->global_store = table->global_store;
    copy->block_scopes = ptrarray_copy_with_items(table->block_scopes, block_scope_copy, block_scope_destroy);
    if (!copy->block_scopes) {
        goto err;
    }
    copy->free_symbols = ptrarray_copy_with_items(table->free_symbols, symbol_copy, symbol_destroy);
    if (!copy->free_symbols) {
        goto err;
    }
    copy->module_global_symbols = ptrarray_copy_with_items(table->module_global_symbols, symbol_copy, symbol_destroy);
    if (!copy->module_global_symbols) {
        goto err;
    }
    copy->max_num_definitions = table->max_num_definitions;
    copy->module_global_offset = table->module_global_offset;
    return copy;
err:
    symbol_table_destroy(copy);
    return NULL;
}

bool symbol_table_add_module_symbol(symbol_table_t *st, symbol_t *symbol) {
    if (symbol->type != SYMBOL_MODULE_GLOBAL) {
        APE_ASSERT(false);
        return false;
    }
    if (symbol_table_symbol_is_defined(st, symbol->name)) {
        return true; // todo: make sure it should be true in this case
    }
    symbol_t *copy = symbol_copy(symbol);
    if (!copy) {
        return false;
    }
    bool ok = set_symbol(st, copy);
    if (!ok) {
        symbol_destroy(copy);
        return false;
    }
    return true;
}

const symbol_t *symbol_table_define(symbol_table_t *table, const char *name, bool assignable) {
    const symbol_t *global_symbol = global_store_get_symbol(table->global_store, name);
    if (global_symbol) {
        return NULL;
    }

    if (strchr(name, ':')) {
        return NULL; // module symbol
    }
    if (APE_STREQ(name, "this")) {
        return NULL; // "this" is reserved
    }
    symbol_type_t symbol_type = table->outer == NULL ? SYMBOL_MODULE_GLOBAL : SYMBOL_LOCAL;
    int ix = next_symbol_index(table);
    symbol_t *symbol = symbol_make(table->alloc, name, symbol_type, ix, assignable);
    if (!symbol) {
        return NULL;
    }

    bool global_symbol_added = false;
    bool ok = false;

    if (symbol_type == SYMBOL_MODULE_GLOBAL && ptrarray_count(table->block_scopes) == 1) {
        symbol_t *global_symbol_copy = symbol_copy(symbol);
        if (!global_symbol_copy) {
            symbol_destroy(symbol);
            return NULL;
        }
        ok = ptrarray_add(table->module_global_symbols, global_symbol_copy);
        if (!ok) {
            symbol_destroy(global_symbol_copy);
            symbol_destroy(symbol);
            return NULL;
        }
        global_symbol_added = true;
    }

    ok = set_symbol(table, symbol);
    if (!ok) {
        if (global_symbol_added) {
            symbol_t *global_symbol_copy = ptrarray_pop(table->module_global_symbols);
            symbol_destroy(global_symbol_copy);
        }
        symbol_destroy(symbol);
        return NULL;
    }

    block_scope_t *top_scope = ptrarray_top(table->block_scopes);
    top_scope->num_definitions++;
    int definitions_count = count_num_definitions(table);
    if (definitions_count > table->max_num_definitions) {
        table->max_num_definitions = definitions_count;
    }

    return symbol;
}

const symbol_t *symbol_table_define_free(symbol_table_t *st, const symbol_t *original) {
    symbol_t *copy = symbol_make(st->alloc, original->name, original->type, original->index, original->assignable);
    if (!copy) {
        return NULL;
    }
    bool ok = ptrarray_add(st->free_symbols, copy);
    if (!ok) {
        symbol_destroy(copy);
        return NULL;
    }

    symbol_t *symbol = symbol_make(st->alloc, original->name, SYMBOL_FREE, ptrarray_count(st->free_symbols) - 1, original->assignable);
    if (!symbol) {
        return NULL;
    }

    ok = set_symbol(st, symbol);
    if (!ok) {
        symbol_destroy(symbol);
        return NULL;
    }

    return symbol;
}

const symbol_t * symbol_table_define_function_name(symbol_table_t *st, const char *name, bool assignable) {
    if (strchr(name, ':')) {
        return NULL; // module symbol
    }
    symbol_t *symbol = symbol_make(st->alloc, name, SYMBOL_FUNCTION, 0, assignable);
    if (!symbol) {
        return NULL;
    }

    bool ok = set_symbol(st, symbol);
    if (!ok) {
        symbol_destroy(symbol);
        return NULL;
    }

    return symbol;
}

const symbol_t *symbol_table_define_this(symbol_table_t *st) {
    symbol_t *symbol = symbol_make(st->alloc, "this", SYMBOL_THIS, 0, false);
    if (!symbol) {
        return NULL;
    }

    bool ok = set_symbol(st, symbol);
    if (!ok) {
        symbol_destroy(symbol);
        return NULL;
    }

    return symbol;
}

const symbol_t *symbol_table_resolve(symbol_table_t *table, const char *name) {
    const symbol_t *symbol = NULL;
    block_scope_t *scope = NULL;

    symbol = global_store_get_symbol(table->global_store, name);
    if (symbol) {
        return symbol;
    }

    for (int i = ptrarray_count(table->block_scopes) - 1; i >= 0; i--) {
        scope = ptrarray_get(table->block_scopes, i);
        symbol = dict_get(scope->store, name);
        if (symbol) {
            break;
        }
    }

    if (symbol && symbol->type == SYMBOL_THIS) {
        symbol = symbol_table_define_free(table, symbol);
    }

    if (!symbol && table->outer) {
        symbol = symbol_table_resolve(table->outer, name);
        if (!symbol) {
            return NULL;
        }
        if (symbol->type == SYMBOL_MODULE_GLOBAL || symbol->type == SYMBOL_APE_GLOBAL) {
            return symbol;
        }
        symbol = symbol_table_define_free(table, symbol);
    }
    return symbol;
}

bool symbol_table_symbol_is_defined(symbol_table_t *table, const char *name) { // todo: rename to something more obvious
    const symbol_t *symbol = global_store_get_symbol(table->global_store, name);
    if (symbol) {
        return true;
    }

    block_scope_t *top_scope = ptrarray_top(table->block_scopes);
    symbol = dict_get(top_scope->store, name);
    if (symbol) {
        return true;
    }
    return false;
}

bool symbol_table_push_block_scope(symbol_table_t *table) {

    int block_scope_offset = 0;
    block_scope_t *prev_block_scope = ptrarray_top(table->block_scopes);
    if (prev_block_scope) {
        block_scope_offset = table->module_global_offset + prev_block_scope->offset + prev_block_scope->num_definitions;
    } else {
        block_scope_offset = table->module_global_offset;
    }

    block_scope_t *new_scope = block_scope_make(table->alloc, block_scope_offset);
    if (!new_scope) {
        return false;
    }
    bool ok = ptrarray_push(table->block_scopes, new_scope);
    if (!ok) {
        block_scope_destroy(new_scope);
        return false;
    }
    return true;
}

void symbol_table_pop_block_scope(symbol_table_t *table) {
    block_scope_t *top_scope = ptrarray_top(table->block_scopes);
    ptrarray_pop(table->block_scopes);
    block_scope_destroy(top_scope);
}

block_scope_t* symbol_table_get_block_scope(symbol_table_t *table) {
    block_scope_t *top_scope = ptrarray_top(table->block_scopes);
    return top_scope;
}

bool symbol_table_is_module_global_scope(symbol_table_t *table) {
    return table->outer == NULL;
}

bool symbol_table_is_top_block_scope(symbol_table_t *table) {
    return ptrarray_count(table->block_scopes) == 1;
}

bool symbol_table_is_top_global_scope(symbol_table_t *table) {
    return symbol_table_is_module_global_scope(table) && symbol_table_is_top_block_scope(table);
}

int symbol_table_get_module_global_symbol_count(const symbol_table_t *table) {
    return ptrarray_count(table->module_global_symbols);
}

const symbol_t * symbol_table_get_module_global_symbol_at(const symbol_table_t *table, int ix) {
    return ptrarray_get(table->module_global_symbols, ix);
}

// INTERNAL
static block_scope_t* block_scope_make(allocator_t *alloc, int offset) {
    block_scope_t *new_scope = allocator_malloc(alloc, sizeof(block_scope_t));
    if (!new_scope) {
        return NULL;
    }
    memset(new_scope, 0, sizeof(block_scope_t));
    new_scope->alloc = alloc;
    new_scope->store = dict_make(alloc, symbol_copy, symbol_destroy);
    if (!new_scope->store) {
        block_scope_destroy(new_scope);
        return NULL;
    }
    new_scope->num_definitions = 0;
    new_scope->offset = offset;
    return new_scope;
}

static void block_scope_destroy(block_scope_t *scope) {
    dict_destroy_with_items(scope->store);
    allocator_free(scope->alloc, scope);
}

static block_scope_t* block_scope_copy(block_scope_t *scope) {
    block_scope_t *copy = allocator_malloc(scope->alloc, sizeof(block_scope_t));
    if (!copy) {
        return NULL;
    }
    memset(copy, 0, sizeof(block_scope_t));
    copy->alloc = scope->alloc;
    copy->num_definitions = scope->num_definitions;
    copy->offset = scope->offset;
    copy->store = dict_copy_with_items(scope->store);
    if (!copy->store) {
        block_scope_destroy(copy);
        return NULL;
    }
    return copy;
}

static bool set_symbol(symbol_table_t *table, symbol_t *symbol) {
    block_scope_t *top_scope = ptrarray_top(table->block_scopes);
    symbol_t *existing = dict_get(top_scope->store, symbol->name);
    if (existing) {
        symbol_destroy(existing);
    }
    return dict_set(top_scope->store, symbol->name, symbol);
}

static int next_symbol_index(symbol_table_t *table) {
    block_scope_t *top_scope = ptrarray_top(table->block_scopes);
    int ix = top_scope->offset + top_scope->num_definitions;
    return ix;
}

static int count_num_definitions(symbol_table_t *table) {
    int count = 0;
    for (int i = ptrarray_count(table->block_scopes) - 1; i >= 0; i--) {
        block_scope_t *scope = ptrarray_get(table->block_scopes, i);
        count += scope->num_definitions;
    }
    return count;
}
