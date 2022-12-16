#ifndef symbol_table_h
#define symbol_table_h

#ifndef APE_AMALGAMATED
#include "common.h"
#include "token.h"
#include "collections.h"
#endif

typedef struct global_store global_store_t;

typedef enum symbol_type {
    SYMBOL_NONE = 0,
    SYMBOL_MODULE_GLOBAL,
    SYMBOL_LOCAL,
    SYMBOL_APE_GLOBAL,
    SYMBOL_FREE,
    SYMBOL_FUNCTION,
    SYMBOL_THIS,
} symbol_type_t;

typedef struct symbol {
    allocator_t *alloc;
    symbol_type_t type;
    char *name;
    int index;
    bool assignable;
} symbol_t;

typedef struct block_scope {
    allocator_t *alloc;
    dict(symbol_t) *store;
    int offset;
    int num_definitions;
} block_scope_t;

typedef struct symbol_table {
    allocator_t *alloc;
    struct symbol_table *outer;
    global_store_t *global_store;
    ptrarray(block_scope_t) *block_scopes;
    ptrarray(symbol_t) *free_symbols;
    ptrarray(symbol_t) *module_global_symbols;
    int max_num_definitions;
    int module_global_offset;
} symbol_table_t;

APE_INTERNAL symbol_t *symbol_make(allocator_t *alloc, const char *name, symbol_type_t type, int index, bool assignable);
APE_INTERNAL void symbol_destroy(symbol_t *symbol);
APE_INTERNAL symbol_t* symbol_copy(symbol_t *symbol);

APE_INTERNAL symbol_table_t *symbol_table_make(allocator_t *alloc, symbol_table_t *outer, global_store_t *global_store, int module_global_offset);
APE_INTERNAL void symbol_table_destroy(symbol_table_t *st);
APE_INTERNAL symbol_table_t* symbol_table_copy(symbol_table_t *st);
APE_INTERNAL bool symbol_table_add_module_symbol(symbol_table_t *st, symbol_t *symbol);
APE_INTERNAL const symbol_t *symbol_table_define(symbol_table_t *st, const char *name, bool assignable);
APE_INTERNAL const symbol_t *symbol_table_define_free(symbol_table_t *st, const symbol_t *original);
APE_INTERNAL const symbol_t *symbol_table_define_function_name(symbol_table_t *st, const char *name, bool assignable);
APE_INTERNAL const symbol_t *symbol_table_define_this(symbol_table_t *st);

APE_INTERNAL const symbol_t *symbol_table_resolve(symbol_table_t *st, const char *name);

APE_INTERNAL bool symbol_table_symbol_is_defined(symbol_table_t *st, const char *name);
APE_INTERNAL bool symbol_table_push_block_scope(symbol_table_t *table);
APE_INTERNAL void symbol_table_pop_block_scope(symbol_table_t *table);
APE_INTERNAL block_scope_t* symbol_table_get_block_scope(symbol_table_t *table);

APE_INTERNAL bool symbol_table_is_module_global_scope(symbol_table_t *table);
APE_INTERNAL bool symbol_table_is_top_block_scope(symbol_table_t *table);
APE_INTERNAL bool symbol_table_is_top_global_scope(symbol_table_t *table);

APE_INTERNAL int symbol_table_get_module_global_symbol_count(const symbol_table_t *table);
APE_INTERNAL const symbol_t * symbol_table_get_module_global_symbol_at(const symbol_table_t *table, int ix);


#endif /* symbol_table_h */
