#ifndef collections_h
#define collections_h

#include <stdbool.h>
#include <stddef.h>

#ifdef COLLECTIONS_AMALGAMATED
#define COLLECTIONS_API static
#else
#define COLLECTIONS_API
#endif

//-----------------------------------------------------------------------------
// Collections
//-----------------------------------------------------------------------------

typedef unsigned long (*collections_hash_fn)(const void* val);
typedef bool (*collections_equals_fn)(const void *a, const void *b);

//-----------------------------------------------------------------------------
// Allocator
//-----------------------------------------------------------------------------

typedef void * (*allocator_malloc_fn)(void *ctx, size_t size);
typedef void (*allocator_free_fn)(void *ctx, void *ptr);

typedef struct allocator {
    allocator_malloc_fn malloc;
    allocator_free_fn free;
    void *ctx;
} allocator_t;

COLLECTIONS_API allocator_t allocator_make(allocator_malloc_fn malloc_fn, allocator_free_fn free_fn, void *ctx);
COLLECTIONS_API void* allocator_malloc(allocator_t *allocator, size_t size);
COLLECTIONS_API void  allocator_free(allocator_t *allocator, void *ptr);

//-----------------------------------------------------------------------------
// Dictionary
//-----------------------------------------------------------------------------

typedef struct dict_ dict_t_;

#define dict(TYPE) dict_t_

typedef void (*dict_item_destroy_fn)(void* item);
typedef void* (*dict_item_copy_fn)(void* item);

#define dict_make(alloc, copy_fn, destroy_fn) dict_make_(alloc, (dict_item_copy_fn)(copy_fn), (dict_item_destroy_fn)(destroy_fn))

COLLECTIONS_API dict_t_*     dict_make_(allocator_t *alloc, dict_item_copy_fn copy_fn, dict_item_destroy_fn destroy_fn);
COLLECTIONS_API void         dict_destroy(dict_t_ *dict);
COLLECTIONS_API void         dict_destroy_with_items(dict_t_ *dict);
COLLECTIONS_API dict_t_*     dict_copy_with_items(dict_t_ *dict);
COLLECTIONS_API bool         dict_set(dict_t_ *dict, const char *key, void *value);
COLLECTIONS_API void *       dict_get(const dict_t_ *dict, const char *key);
COLLECTIONS_API void *       dict_get_value_at(const dict_t_ *dict, unsigned int ix);
COLLECTIONS_API const char * dict_get_key_at(const dict_t_ *dict, unsigned int ix);
COLLECTIONS_API int          dict_count(const dict_t_ *dict);
COLLECTIONS_API bool         dict_remove(dict_t_ *dict, const char *key);

//-----------------------------------------------------------------------------
// Value dictionary
//-----------------------------------------------------------------------------

typedef struct valdict_ valdict_t_;

#define valdict(KEY_TYPE, VALUE_TYPE) valdict_t_

#define valdict_make(allocator, key_type, val_type) valdict_make_(allocator, sizeof(key_type), sizeof(val_type))

COLLECTIONS_API valdict_t_*  valdict_make_(allocator_t *alloc, size_t key_size, size_t val_size);
COLLECTIONS_API valdict_t_*  valdict_make_with_capacity(allocator_t *alloc, unsigned int min_capacity, size_t key_size, size_t val_size);
COLLECTIONS_API void         valdict_destroy(valdict_t_ *dict);
COLLECTIONS_API void         valdict_set_hash_function(valdict_t_ *dict, collections_hash_fn hash_fn);
COLLECTIONS_API void         valdict_set_equals_function(valdict_t_ *dict, collections_equals_fn equals_fn);
COLLECTIONS_API bool         valdict_set(valdict_t_ *dict, void *key, void *value);
COLLECTIONS_API void *       valdict_get(const valdict_t_ *dict, const void *key);
COLLECTIONS_API void *       valdict_get_key_at(const valdict_t_ *dict, unsigned int ix);
COLLECTIONS_API void *       valdict_get_value_at(const valdict_t_ *dict, unsigned int ix);
COLLECTIONS_API unsigned int valdict_get_capacity(const valdict_t_ *dict);
COLLECTIONS_API bool         valdict_set_value_at(const valdict_t_ *dict, unsigned int ix, const void *value);
COLLECTIONS_API int          valdict_count(const valdict_t_ *dict);
COLLECTIONS_API bool         valdict_remove(valdict_t_ *dict, void *key);
COLLECTIONS_API void         valdict_clear(valdict_t_ *dict);

//-----------------------------------------------------------------------------
// Pointer dictionary
//-----------------------------------------------------------------------------

typedef struct ptrdict_ ptrdict_t_;

#define ptrdict(KEY_TYPE, VALUE_TYPE) ptrdict_t_

COLLECTIONS_API ptrdict_t_* ptrdict_make(allocator_t *alloc);
COLLECTIONS_API void        ptrdict_destroy(ptrdict_t_ *dict);
COLLECTIONS_API void        ptrdict_set_hash_function(ptrdict_t_ *dict, collections_hash_fn hash_fn);
COLLECTIONS_API void        ptrdict_set_equals_function(ptrdict_t_ *dict, collections_equals_fn equals_fn);
COLLECTIONS_API bool        ptrdict_set(ptrdict_t_ *dict, void *key, void *value);
COLLECTIONS_API void *      ptrdict_get(const ptrdict_t_ *dict, const void *key);
COLLECTIONS_API void *      ptrdict_get_value_at(const ptrdict_t_ *dict, unsigned int ix);
COLLECTIONS_API void *      ptrdict_get_key_at(const ptrdict_t_ *dict, unsigned int ix);
COLLECTIONS_API int         ptrdict_count(const ptrdict_t_ *dict);
COLLECTIONS_API bool        ptrdict_remove(ptrdict_t_ *dict, void *key);

//-----------------------------------------------------------------------------
// Array
//-----------------------------------------------------------------------------

typedef struct array_ array_t_;

#define array(TYPE) array_t_

#define array_make(allocator, type) array_make_(allocator, sizeof(type))
typedef void (*array_item_deinit_fn)(void* item);
#define array_destroy_with_items(arr, fn) array_destroy_with_items_(arr, (array_item_deinit_fn)(fn))
#define array_clear_and_deinit_items(arr, fn) array_clear_and_deinit_items_(arr, (array_item_deinit_fn)(fn))

COLLECTIONS_API array_t_*    array_make_(allocator_t *alloc, size_t element_size);
COLLECTIONS_API array_t_*    array_make_with_capacity(allocator_t *alloc, unsigned int capacity, size_t element_size);
COLLECTIONS_API void         array_destroy(array_t_ *arr);
COLLECTIONS_API void         array_destroy_with_items_(array_t_ *arr, array_item_deinit_fn deinit_fn);
COLLECTIONS_API array_t_*    array_copy(const array_t_ *arr);
COLLECTIONS_API bool         array_add(array_t_ *arr, const void *value);
COLLECTIONS_API bool         array_addn(array_t_ *arr, const void *values, int n);
COLLECTIONS_API bool         array_add_array(array_t_ *dest, array_t_ *source);
COLLECTIONS_API bool         array_push(array_t_ *arr, const void *value);
COLLECTIONS_API bool         array_pop(array_t_ *arr, void *out_value);
COLLECTIONS_API void *       array_top(array_t_ *arr);
COLLECTIONS_API bool         array_set(array_t_ *arr, unsigned int ix, void *value);
COLLECTIONS_API bool         array_setn(array_t_ *arr, unsigned int ix, void *values, int n);
COLLECTIONS_API void *       array_get(array_t_ *arr, unsigned int ix);
COLLECTIONS_API const void * array_get_const(const array_t_ *arr, unsigned int ix);
COLLECTIONS_API void *       array_get_last(array_t_ *arr);
COLLECTIONS_API int          array_count(const array_t_ *arr);
COLLECTIONS_API unsigned int array_get_capacity(const array_t_ *arr);
COLLECTIONS_API bool         array_remove_at(array_t_ *arr, unsigned int ix);
COLLECTIONS_API bool         array_remove_item(array_t_ *arr, void *ptr);
COLLECTIONS_API void         array_clear(array_t_ *arr);
COLLECTIONS_API void         array_clear_and_deinit_items_(array_t_ *arr, array_item_deinit_fn deinit_fn);
COLLECTIONS_API void         array_lock_capacity(array_t_ *arr);
COLLECTIONS_API int          array_get_index(const array_t_ *arr, void *ptr);
COLLECTIONS_API bool         array_contains(const array_t_ *arr, void *ptr);
COLLECTIONS_API void*        array_data(array_t_ *arr); // might become invalidated by remove/add operations
COLLECTIONS_API const void*  array_const_data(const array_t_ *arr);
COLLECTIONS_API void         array_orphan_data(array_t_ *arr);
COLLECTIONS_API bool         array_reverse(array_t_ *arr);

//-----------------------------------------------------------------------------
// Pointer Array
//-----------------------------------------------------------------------------

typedef struct ptrarray_ ptrarray_t_;

#define ptrarray(TYPE) ptrarray_t_

typedef void (*ptrarray_item_destroy_fn)(void* item);
#define ptrarray_destroy_with_items(arr, fn) ptrarray_destroy_with_items_(arr, (ptrarray_item_destroy_fn)(fn))
#define ptrarray_clear_and_destroy_items(arr, fn) ptrarray_clear_and_destroy_items_(arr, (ptrarray_item_destroy_fn)(fn))

typedef void* (*ptrarray_item_copy_fn)(void* item);
#define ptrarray_copy_with_items(arr, copy_fn, destroy_fn) ptrarray_copy_with_items_(arr, (ptrarray_item_copy_fn)(copy_fn), (ptrarray_item_destroy_fn)(destroy_fn))

COLLECTIONS_API ptrarray_t_* ptrarray_make(allocator_t *alloc);
COLLECTIONS_API ptrarray_t_* ptrarray_make_with_capacity(allocator_t *alloc, unsigned int capacity);
COLLECTIONS_API void         ptrarray_destroy(ptrarray_t_ *arr);
COLLECTIONS_API void         ptrarray_destroy_with_items_(ptrarray_t_ *arr, ptrarray_item_destroy_fn destroy_fn);
COLLECTIONS_API ptrarray_t_* ptrarray_copy(ptrarray_t_ *arr);
COLLECTIONS_API ptrarray_t_* ptrarray_copy_with_items_(ptrarray_t_ *arr, ptrarray_item_copy_fn copy_fn, ptrarray_item_destroy_fn destroy_fn);
COLLECTIONS_API bool         ptrarray_add(ptrarray_t_ *arr, void *ptr);
COLLECTIONS_API bool         ptrarray_set(ptrarray_t_ *arr, unsigned int ix, void *ptr);
COLLECTIONS_API bool         ptrarray_add_array(ptrarray_t_ *dest, ptrarray_t_ *source);
COLLECTIONS_API void *       ptrarray_get(ptrarray_t_ *arr, unsigned int ix);
COLLECTIONS_API const void * ptrarray_get_const(const ptrarray_t_ *arr, unsigned int ix);
COLLECTIONS_API bool         ptrarray_push(ptrarray_t_ *arr, void *ptr);
COLLECTIONS_API void *       ptrarray_pop(ptrarray_t_ *arr);
COLLECTIONS_API void *       ptrarray_top(ptrarray_t_ *arr);
COLLECTIONS_API int          ptrarray_count(const ptrarray_t_ *arr);
COLLECTIONS_API bool         ptrarray_remove_at(ptrarray_t_ *arr, unsigned int ix);
COLLECTIONS_API bool         ptrarray_remove_item(ptrarray_t_ *arr, void *item);
COLLECTIONS_API void         ptrarray_clear(ptrarray_t_ *arr);
COLLECTIONS_API void         ptrarray_clear_and_destroy_items_(ptrarray_t_ *arr, ptrarray_item_destroy_fn destroy_fn);
COLLECTIONS_API void         ptrarray_lock_capacity(ptrarray_t_ *arr);
COLLECTIONS_API int          ptrarray_get_index(ptrarray_t_ *arr, void *ptr);
COLLECTIONS_API bool         ptrarray_contains(ptrarray_t_ *arr, void *ptr);
COLLECTIONS_API void *       ptrarray_get_addr(ptrarray_t_ *arr, unsigned int ix);
COLLECTIONS_API void*        ptrarray_data(ptrarray_t_ *arr); // might become invalidated by remove/add operations
COLLECTIONS_API void         ptrarray_reverse(ptrarray_t_ *arr);

//-----------------------------------------------------------------------------
// String buffer
//-----------------------------------------------------------------------------

typedef struct strbuf strbuf_t;

COLLECTIONS_API strbuf_t* strbuf_make(allocator_t *alloc);
COLLECTIONS_API strbuf_t* strbuf_make_with_capacity(allocator_t *alloc, unsigned int capacity);
COLLECTIONS_API void strbuf_destroy(strbuf_t *buf);
COLLECTIONS_API void strbuf_clear(strbuf_t *buf);
COLLECTIONS_API bool strbuf_append(strbuf_t *buf, const char *str);
COLLECTIONS_API bool strbuf_appendf(strbuf_t *buf, const char *fmt, ...)  __attribute__((format(printf, 2, 3)));
COLLECTIONS_API const char * strbuf_get_string(const strbuf_t *buf);
COLLECTIONS_API size_t strbuf_get_length(const strbuf_t *buf);
COLLECTIONS_API char * strbuf_get_string_and_destroy(strbuf_t *buf);
COLLECTIONS_API bool strbuf_failed(strbuf_t *buf);

//-----------------------------------------------------------------------------
// Utils
//-----------------------------------------------------------------------------

COLLECTIONS_API ptrarray(char)* kg_split_string(allocator_t *alloc, const char *str, const char *delimiter);
COLLECTIONS_API char* kg_join(allocator_t *alloc, ptrarray(char) *items, const char *with);
COLLECTIONS_API char* kg_canonicalise_path(allocator_t *alloc, const char *path);
COLLECTIONS_API bool  kg_is_path_absolute(const char *path);
COLLECTIONS_API bool  kg_streq(const char *a, const char *b);

#endif /* collections_h */
