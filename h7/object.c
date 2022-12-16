#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <float.h>
#include <math.h>

#ifndef APE_AMALGAMATED
#include "object.h"
#include "code.h"
#include "compiler.h"
#include "traceback.h"
#include "gc.h"
#endif

#define OBJECT_PATTERN          0xfff8000000000000 //data
#define OBJECT_HEADER_MASK      0xffff000000000000
#define OBJECT_ALLOCATED_HEADER 0xfffc000000000000
#define OBJECT_BOOL_HEADER      0xfff9000000000000
#define OBJECT_NULL_PATTERN     0xfffa000000000000

static object_t object_deep_copy_internal(gcmem_t *mem, object_t obj, valdict(object_t, object_t) *copies);
static bool object_equals_wrapped(const object_t *a, const object_t *b);
static unsigned long object_hash(object_t *obj_ptr);
static unsigned long object_hash_string(const char *str);
static unsigned long object_hash_double(double val);
static array(object_t)* object_get_allocated_array(object_t object);
static bool object_is_number(object_t obj);
static uint64_t get_type_tag(object_type_t type);
static bool freevals_are_allocated(function_t *fun);
static char *object_data_get_string(object_data_t *data);
static bool object_data_string_reserve_capacity(object_data_t *data, int capacity);

object_t object_make_from_data(object_type_t type, object_data_t *data) {
    object_t object;
    object.handle = OBJECT_PATTERN;
    uint64_t type_tag = get_type_tag(type) & 0x7;
    object.handle |= (type_tag << 48);
    object.handle |= (uintptr_t)data; // assumes no pointer exceeds 48 bits
    return object;
}

object_t object_make_number(double val) {
    object_t o = { .number = val };
    if ((o.handle & OBJECT_PATTERN) == OBJECT_PATTERN) {
        o.handle = 0x7ff8000000000000;
    }
    return o;
}

object_t object_make_bool(bool val) {
    return (object_t) { .handle = OBJECT_BOOL_HEADER | val };
}

object_t object_make_null() {
    return (object_t) { .handle = OBJECT_NULL_PATTERN };
}

object_t object_make_string(gcmem_t *mem, const char *string) {
    int len = (int)strlen(string);
    object_t res = object_make_string_with_capacity(mem, len);
    if (object_is_null(res)) {
        return res;
    }
    bool ok = object_string_append(res, string, len);
    if (!ok) {
        return object_make_null();
    }
    return res;
}

object_t object_make_string_with_capacity(gcmem_t *mem, int capacity) {
    object_data_t *data = gcmem_get_object_data_from_pool(mem, OBJECT_STRING);
    if (!data) {
        data = gcmem_alloc_object_data(mem, OBJECT_STRING);
        if (!data) {
            return object_make_null();
        }
        data->string.capacity = OBJECT_STRING_BUF_SIZE - 1;
        data->string.is_allocated = false;
    }

    data->string.length = 0;
    data->string.hash = 0;

    if (capacity > data->string.capacity) {
        bool ok = object_data_string_reserve_capacity(data, capacity);
        if (!ok) {
            return object_make_null();
        }
    }

    return object_make_from_data(OBJECT_STRING, data);
}

object_t object_make_stringf(gcmem_t *mem, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int to_write = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    va_start(args, fmt);
    object_t res = object_make_string_with_capacity(mem, to_write);
    if (object_is_null(res)) {
        return object_make_null();
    }
    char *res_buf = object_get_mutable_string(res);
    int written = vsprintf(res_buf, fmt, args);
    (void)written;
    APE_ASSERT(written == to_write);
    va_end(args);
    object_set_string_length(res, to_write);
    return res;
}

object_t object_make_native_function(gcmem_t *mem, const char *name, native_fn fn, void *data, int data_len) {
    if (data_len > NATIVE_FN_MAX_DATA_LEN) {
        return object_make_null();
    }
    object_data_t *obj = gcmem_alloc_object_data(mem, OBJECT_NATIVE_FUNCTION);
    if (!obj) {
        return object_make_null();
    }
    obj->native_function.name = ape_strdup(mem->alloc, name);
    if (!obj->native_function.name) {
        return object_make_null();
    }
    obj->native_function.fn = fn;
    if (data) {
        memcpy(obj->native_function.data, data, data_len);
    }
    obj->native_function.data_len = data_len;
    return object_make_from_data(OBJECT_NATIVE_FUNCTION, obj);
}

object_t object_make_array(gcmem_t *mem) {
    return object_make_array_with_capacity(mem, 8);
}

object_t object_make_array_with_capacity(gcmem_t *mem, unsigned capacity) {
    object_data_t *data = gcmem_get_object_data_from_pool(mem, OBJECT_ARRAY);
    if (data) {
        array_clear(data->array);
        return object_make_from_data(OBJECT_ARRAY, data);
    }
    data = gcmem_alloc_object_data(mem, OBJECT_ARRAY);
    if (!data) {
        return object_make_null();
    }
    data->array = array_make_with_capacity(mem->alloc, capacity, sizeof(object_t));
    if (!data->array) {
        return object_make_null();
    }
    return object_make_from_data(OBJECT_ARRAY, data);
}

object_t object_make_map(gcmem_t *mem) {
    return object_make_map_with_capacity(mem, 32);
}

object_t object_make_map_with_capacity(gcmem_t *mem, unsigned capacity) {
    object_data_t *data = gcmem_get_object_data_from_pool(mem, OBJECT_MAP);
    if (data) {
        valdict_clear(data->map);
        return object_make_from_data(OBJECT_MAP, data);
    }
    data = gcmem_alloc_object_data(mem, OBJECT_MAP);
    if (!data) {
        return object_make_null();
    }
    data->map = valdict_make_with_capacity(mem->alloc, capacity, sizeof(object_t), sizeof(object_t));
    if (!data->map) {
        return object_make_null();
    }
    valdict_set_hash_function(data->map, (collections_hash_fn)object_hash);
    valdict_set_equals_function(data->map, (collections_equals_fn)object_equals_wrapped);
    return object_make_from_data(OBJECT_MAP, data);
}

object_t object_make_error(gcmem_t *mem, const char *error) {
    char *error_str = ape_strdup(mem->alloc, error);
    if (!error_str) {
        return object_make_null();
    }
    object_t res = object_make_error_no_copy(mem, error_str);
    if (object_is_null(res)) {
        allocator_free(mem->alloc, error_str);
        return object_make_null();
    }
    return res;
}

object_t object_make_error_no_copy(gcmem_t *mem, char *error) {
    object_data_t *data = gcmem_alloc_object_data(mem, OBJECT_ERROR);
    if (!data) {
        return object_make_null();
    }
    data->error.message = error;
    data->error.traceback = NULL;
    return object_make_from_data(OBJECT_ERROR, data);
}

object_t object_make_errorf(gcmem_t *mem, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int to_write = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    va_start(args, fmt);
    char *res = (char*)allocator_malloc(mem->alloc, to_write + 1);
    if (!res) {
        return object_make_null();
    }
    int written = vsprintf(res, fmt, args);
    (void)written;
    APE_ASSERT(written == to_write);
    va_end(args);
    object_t res_obj = object_make_error_no_copy(mem, res);
    if (object_is_null(res_obj)) {
        allocator_free(mem->alloc, res);
        return object_make_null();
    }
    return res_obj;
}

object_t object_make_function(gcmem_t *mem, const char *name, compilation_result_t *comp_res, bool owns_data,
                              int num_locals, int num_args,
                              int free_vals_count) {

    object_data_t *data = gcmem_alloc_object_data(mem, OBJECT_FUNCTION);
    if (!data) {
        return object_make_null();
    }
    if (owns_data) {
        data->function.name = name ? ape_strdup(mem->alloc, name) : ape_strdup(mem->alloc, "anonymous");
        if (!data->function.name) {
            return object_make_null();
        }
    } else {
        data->function.const_name = name ? name : "anonymous";
    }
    data->function.comp_result = comp_res;
    data->function.owns_data = owns_data;
    data->function.num_locals = num_locals;
    data->function.num_args = num_args;
    if (free_vals_count >= APE_ARRAY_LEN(data->function.free_vals_buf)) {
        data->function.free_vals_allocated = allocator_malloc(mem->alloc, sizeof(object_t) * free_vals_count);
        if (!data->function.free_vals_allocated) {
            return object_make_null();
        }
    }
    data->function.free_vals_count = free_vals_count;
    return object_make_from_data(OBJECT_FUNCTION, data);
}

object_t object_make_external(gcmem_t *mem, void *data) {
    object_data_t *obj = gcmem_alloc_object_data(mem, OBJECT_EXTERNAL);
    if (!obj) {
        return object_make_null();
    }
    obj->external.data = data;
    obj->external.data_destroy_fn = NULL;
    obj->external.data_copy_fn = NULL;
    return object_make_from_data(OBJECT_EXTERNAL, obj);
}

void object_deinit(object_t obj) {
    if (object_is_allocated(obj)) {
        object_data_t *data = object_get_allocated_data(obj);
        object_data_deinit(data);
    }
}

void object_data_deinit(object_data_t *data) {
    switch (data->type) {
        case OBJECT_FREED: {
            APE_ASSERT(false);
            return;
        }
        case OBJECT_STRING: {
            if (data->string.is_allocated) {
                allocator_free(data->mem->alloc, data->string.value_allocated);
            }
            break;
        }
        case OBJECT_FUNCTION: {
            if (data->function.owns_data) {
                allocator_free(data->mem->alloc, data->function.name);
                compilation_result_destroy(data->function.comp_result);
            }
            if (freevals_are_allocated(&data->function)) {
                allocator_free(data->mem->alloc, data->function.free_vals_allocated);
            }
            break;
        }
        case OBJECT_ARRAY: {
            array_destroy(data->array);
            break;
        }
        case OBJECT_MAP: {
            valdict_destroy(data->map);
            break;
        }
        case OBJECT_NATIVE_FUNCTION: {
            allocator_free(data->mem->alloc, data->native_function.name);
            break;
        }
        case OBJECT_EXTERNAL: {
            if (data->external.data_destroy_fn) {
                data->external.data_destroy_fn(data->external.data);
            }
            break;
        }
        case OBJECT_ERROR: {
            allocator_free(data->mem->alloc, data->error.message);
            traceback_destroy(data->error.traceback);
            break;
        }
        default: {
            break;
        }
    }
    data->type = OBJECT_FREED;
}

bool object_is_allocated(object_t object) {
    return (object.handle & OBJECT_ALLOCATED_HEADER) == OBJECT_ALLOCATED_HEADER;
}

gcmem_t* object_get_mem(object_t obj) {
    object_data_t *data = object_get_allocated_data(obj);
    return data->mem;
}

bool object_is_hashable(object_t obj) {
    object_type_t type = object_get_type(obj);
    switch (type) {
        case OBJECT_STRING: return true;
        case OBJECT_NUMBER: return true;
        case OBJECT_BOOL:   return true;
        default: return false;
    }
}

void object_to_string(object_t obj, strbuf_t *buf, bool quote_str) {
    object_type_t type = object_get_type(obj);
    switch (type) {
        case OBJECT_FREED: {
            strbuf_append(buf, "FREED");
            break;
        }
        case OBJECT_NONE: {
            strbuf_append(buf, "NONE");
            break;
        }
        case OBJECT_NUMBER: {
            double number = object_get_number(obj);
            strbuf_appendf(buf, "%1.10g", number);
            break;
        }
        case OBJECT_BOOL: {
            strbuf_append(buf, object_get_bool(obj) ? "true" : "false");
            break;
        }
        case OBJECT_STRING: {
            const char *string = object_get_string(obj);
            if (quote_str) {
                strbuf_appendf(buf, "\"%s\"", string);
            } else {
                strbuf_append(buf, string);
            }
            break;
        }
        case OBJECT_NULL: {
            strbuf_append(buf, "null");
            break;
        }
        case OBJECT_FUNCTION: {
            const function_t *function = object_get_function(obj);
            strbuf_appendf(buf, "CompiledFunction: %s\n", object_get_function_name(obj));
            code_to_string(function->comp_result->bytecode, function->comp_result->src_positions, function->comp_result->count, buf);
            break;
        }
        case OBJECT_ARRAY: {
            strbuf_append(buf, "[");
            for (int i = 0; i < object_get_array_length(obj); i++) {
                object_t iobj = object_get_array_value_at(obj, i);
                object_to_string(iobj, buf, true);
                if (i < (object_get_array_length(obj) - 1)) {
                    strbuf_append(buf, ", ");
                }
            }
            strbuf_append(buf, "]");
            break;
        }
        case OBJECT_MAP: {
            strbuf_append(buf, "{");
            for (int i = 0; i < object_get_map_length(obj); i++) {
                object_t key = object_get_map_key_at(obj, i);
                object_t val = object_get_map_value_at(obj, i);
                object_to_string(key, buf, true);
                strbuf_append(buf, ": ");
                object_to_string(val, buf, true);
                if (i < (object_get_map_length(obj) - 1)) {
                    strbuf_append(buf, ", ");
                }
            }
            strbuf_append(buf, "}");
            break;
        }
        case OBJECT_NATIVE_FUNCTION: {
            strbuf_append(buf, "NATIVE_FUNCTION");
            break;
        }
        case OBJECT_EXTERNAL: {
            strbuf_append(buf, "EXTERNAL");
            break;
        }
        case OBJECT_ERROR: {
            strbuf_appendf(buf, "ERROR: %s\n", object_get_error_message(obj));
            traceback_t *traceback = object_get_error_traceback(obj);
            APE_ASSERT(traceback);
            if (traceback) {
                strbuf_append(buf, "Traceback:\n");
                traceback_to_string(traceback, buf);
            }
            break;
        }
        case OBJECT_ANY: {
            APE_ASSERT(false);
        }
    }
}

const char *object_get_type_name(const object_type_t type) {
    switch (type) {
        case OBJECT_NONE:            return "NONE";
        case OBJECT_FREED:           return "NONE";
        case OBJECT_NUMBER:          return "NUMBER";
        case OBJECT_BOOL:            return "BOOL";
        case OBJECT_STRING:          return "STRING";
        case OBJECT_NULL:            return "NULL";
        case OBJECT_NATIVE_FUNCTION: return "NATIVE_FUNCTION";
        case OBJECT_ARRAY:           return "ARRAY";
        case OBJECT_MAP:             return "MAP";
        case OBJECT_FUNCTION:        return "FUNCTION";
        case OBJECT_EXTERNAL:        return "EXTERNAL";
        case OBJECT_ERROR:           return "ERROR";
        case OBJECT_ANY:             return "ANY";
    }
    return "NONE";
}

char* object_get_type_union_name(allocator_t *alloc, const object_type_t type) {
    if (type == OBJECT_ANY || type == OBJECT_NONE || type == OBJECT_FREED) {
        return ape_strdup(alloc, object_get_type_name(type));
    }
    strbuf_t *res = strbuf_make(alloc);
    if (!res) {
        return NULL;
    }
    bool in_between = false;
#define CHECK_TYPE(t) \
    do {\
        if ((type & t) == t) {\
            if (in_between) {\
                strbuf_append(res, "|");\
            }\
            strbuf_append(res, object_get_type_name(t));\
            in_between = true;\
        }\
    } while (0)

    CHECK_TYPE(OBJECT_NUMBER);
    CHECK_TYPE(OBJECT_BOOL);
    CHECK_TYPE(OBJECT_STRING);
    CHECK_TYPE(OBJECT_NULL);
    CHECK_TYPE(OBJECT_NATIVE_FUNCTION);
    CHECK_TYPE(OBJECT_ARRAY);
    CHECK_TYPE(OBJECT_MAP);
    CHECK_TYPE(OBJECT_FUNCTION);
    CHECK_TYPE(OBJECT_EXTERNAL);
    CHECK_TYPE(OBJECT_ERROR);

    return strbuf_get_string_and_destroy(res);
}

char* object_serialize(allocator_t *alloc, object_t object) {
    strbuf_t *buf = strbuf_make(alloc);
    if (!buf) {
        return NULL;
    }
    object_to_string(object, buf, true);
    char *string = strbuf_get_string_and_destroy(buf);
    return string;
}

object_t object_deep_copy(gcmem_t *mem, object_t obj) {
    valdict(object_t, object_t) *copies = valdict_make(mem->alloc, object_t, object_t);
    if (!copies) {
        return object_make_null();
    }
    object_t res = object_deep_copy_internal(mem, obj, copies);
    valdict_destroy(copies);
    return res;
}

object_t object_copy(gcmem_t *mem, object_t obj) {
    object_t copy = object_make_null();
    object_type_t type = object_get_type(obj);
    switch (type) {
        case OBJECT_ANY:
        case OBJECT_FREED:
        case OBJECT_NONE: {
            APE_ASSERT(false);
            copy = object_make_null();
            break;
        }
        case OBJECT_NUMBER:
        case OBJECT_BOOL:
        case OBJECT_NULL:
        case OBJECT_FUNCTION:
        case OBJECT_NATIVE_FUNCTION:
        case OBJECT_ERROR: {
            copy = obj;
            break;
        }
        case OBJECT_STRING: {
            const char *str = object_get_string(obj);
            copy = object_make_string(mem, str);
            break;
        }
        case OBJECT_ARRAY: {
            int len = object_get_array_length(obj);
            copy = object_make_array_with_capacity(mem, len);
            if (object_is_null(copy)) {
                return object_make_null();
            }
            for (int i = 0; i < len; i++) {
                object_t item = object_get_array_value_at(obj, i);
                bool ok = object_add_array_value(copy, item);
                if (!ok) {
                    return object_make_null();
                }
            }
            break;
        }
        case OBJECT_MAP: {
            copy = object_make_map(mem);
            for (int i = 0; i < object_get_map_length(obj); i++) {
                object_t key = object_get_map_key_at(obj, i);
                object_t val = object_get_map_value_at(obj, i);
                bool ok = object_set_map_value(copy, key, val);
                if (!ok) {
                    return object_make_null();
                }
            }
            break;
        }
        case OBJECT_EXTERNAL: {
            copy = object_make_external(mem, NULL);
            if (object_is_null(copy)) {
                return object_make_null();
            }
            external_data_t *external = object_get_external_data(obj);
            void *data_copy = NULL;
            if (external->data_copy_fn) {
                data_copy = external->data_copy_fn(external->data);
            } else {
                data_copy = external->data;
            }
            object_set_external_data(copy, data_copy);
            object_set_external_destroy_function(copy, external->data_destroy_fn);
            object_set_external_copy_function(copy, external->data_copy_fn);
            break;
        }
    }
    return copy;
}

double object_compare(object_t a, object_t b, bool *out_ok) {
    if (a.handle == b.handle) {
        return 0;
    }

    *out_ok = true;

    object_type_t a_type = object_get_type(a);
    object_type_t b_type = object_get_type(b);

    if ((a_type == OBJECT_NUMBER || a_type == OBJECT_BOOL || a_type == OBJECT_NULL)
        && (b_type == OBJECT_NUMBER || b_type == OBJECT_BOOL || b_type == OBJECT_NULL)) {
        double left_val = object_get_number(a);
        double right_val = object_get_number(b);
        return left_val - right_val;
    } else if (a_type == b_type && a_type == OBJECT_STRING) {
        int a_len = object_get_string_length(a);
        int b_len = object_get_string_length(b);
        if (a_len != b_len) {
            return a_len - b_len;
        }
        unsigned long a_hash = object_get_string_hash(a);
        unsigned long b_hash = object_get_string_hash(b);
        if (a_hash != b_hash) {
            return a_hash - b_hash;
        }
        const char *a_string = object_get_string(a);
        const char *b_string = object_get_string(b);
        return strcmp(a_string, b_string);
    } else if ((object_is_allocated(a) || object_is_null(a))
            && (object_is_allocated(b) || object_is_null(b))) {
        intptr_t a_data_val = (intptr_t)object_get_allocated_data(a);
        intptr_t b_data_val = (intptr_t)object_get_allocated_data(b);
        return (double)(a_data_val - b_data_val);
    } else {
        *out_ok = false;
    }
    return 1;
}

bool object_equals(object_t a, object_t b) {
    object_type_t a_type = object_get_type(a);
    object_type_t b_type = object_get_type(b);

    if (a_type != b_type) {
        return false;
    }
    bool ok = false;
    double res = object_compare(a, b, &ok);
    return APE_DBLEQ(res, 0);
}

external_data_t* object_get_external_data(object_t object) {
    APE_ASSERT(object_get_type(object) == OBJECT_EXTERNAL);
    object_data_t *data = object_get_allocated_data(object);
    return &data->external;
}

bool object_set_external_destroy_function(object_t object, external_data_destroy_fn destroy_fn) {
    APE_ASSERT(object_get_type(object) == OBJECT_EXTERNAL);
    external_data_t* data = object_get_external_data(object);
    if (!data) {
        return false;
    }
    data->data_destroy_fn = destroy_fn;
    return true;
}

object_data_t* object_get_allocated_data(object_t object) {
    APE_ASSERT(object_is_allocated(object) || object_get_type(object) == OBJECT_NULL);
    return (object_data_t*)(object.handle & ~OBJECT_HEADER_MASK);
}

bool object_get_bool(object_t obj) {
    if (object_is_number(obj)) {
        return obj.handle;
    }
    return obj.handle & (~OBJECT_HEADER_MASK);
}

double object_get_number(object_t obj) {
    if (object_is_number(obj)) { // todo: optimise? always return number?
        return obj.number;
    }
    return (double)(obj.handle & (~OBJECT_HEADER_MASK));
}

const char * object_get_string(object_t object) {
    APE_ASSERT(object_get_type(object) == OBJECT_STRING);
    object_data_t *data = object_get_allocated_data(object);
    return object_data_get_string(data);
}

int object_get_string_length(object_t object) {
    APE_ASSERT(object_get_type(object) == OBJECT_STRING);
    object_data_t *data = object_get_allocated_data(object);
    return data->string.length;
}

void object_set_string_length(object_t object, int len) {
    APE_ASSERT(object_get_type(object) == OBJECT_STRING);
    object_data_t *data = object_get_allocated_data(object);
    data->string.length = len;
}


int object_get_string_capacity(object_t object) {
    APE_ASSERT(object_get_type(object) == OBJECT_STRING);
    object_data_t *data = object_get_allocated_data(object);
    return data->string.capacity;
}

char* object_get_mutable_string(object_t object) {
    APE_ASSERT(object_get_type(object) == OBJECT_STRING);
    object_data_t *data = object_get_allocated_data(object);
    return object_data_get_string(data);
}

bool object_string_append(object_t obj, const char *src, int len) {
    APE_ASSERT(object_get_type(obj) == OBJECT_STRING);
    object_data_t *data = object_get_allocated_data(obj);
    object_string_t *string = &data->string;
    char *str_buf = object_get_mutable_string(obj);
    int current_len = string->length;
    int capacity = string->capacity;
    if ((len + current_len) > capacity) {
        APE_ASSERT(false);
        return false;
    }
    memcpy(str_buf + current_len, src, len);
    string->length += len;
    str_buf[string->length] = '\0';
    return true;
}

unsigned long object_get_string_hash(object_t obj) {
    APE_ASSERT(object_get_type(obj) == OBJECT_STRING);
    object_data_t *data = object_get_allocated_data(obj);
    if (data->string.hash == 0) {
        data->string.hash = object_hash_string(object_get_string(obj));
        if (data->string.hash == 0) {
            data->string.hash = 1;
        }
    }
    return data->string.hash;
}

function_t* object_get_function(object_t object) {
    APE_ASSERT(object_get_type(object) == OBJECT_FUNCTION);
    object_data_t *data = object_get_allocated_data(object);
    return &data->function;
}

native_function_t* object_get_native_function(object_t obj) {
    object_data_t *data = object_get_allocated_data(obj);
    return &data->native_function;
}

object_type_t object_get_type(object_t obj) {
    if (object_is_number(obj)) {
        return OBJECT_NUMBER;
    }
    uint64_t tag = (obj.handle >> 48) & 0x7;
    switch (tag) {
        case 0: return OBJECT_NONE;
        case 1: return OBJECT_BOOL;
        case 2: return OBJECT_NULL;
        case 4: {
            object_data_t *data = object_get_allocated_data(obj);
            return data->type;
        }
        default: return OBJECT_NONE;
    }
}

bool object_is_numeric(object_t obj) {
    object_type_t type = object_get_type(obj);
    return type == OBJECT_NUMBER || type == OBJECT_BOOL;
}

bool object_is_null(object_t obj) {
    return object_get_type(obj) == OBJECT_NULL;
}

bool object_is_callable(object_t obj) {
    object_type_t type = object_get_type(obj);
    return type == OBJECT_NATIVE_FUNCTION || type == OBJECT_FUNCTION;
}

const char* object_get_function_name(object_t obj) {
    APE_ASSERT(object_get_type(obj) == OBJECT_FUNCTION);
    object_data_t *data = object_get_allocated_data(obj);
    APE_ASSERT(data);
    if (!data) {
        return NULL;
    }

    if (data->function.owns_data) {
        return data->function.name;
    } else {
        return data->function.const_name;
    }
}

object_t object_get_function_free_val(object_t obj, int ix) {
    APE_ASSERT(object_get_type(obj) == OBJECT_FUNCTION);
    object_data_t *data = object_get_allocated_data(obj);
    APE_ASSERT(data);
    if (!data) {
        return object_make_null();
    }
    function_t *fun = &data->function;
    APE_ASSERT(ix >= 0 && ix < fun->free_vals_count);
    if (ix < 0 || ix >= fun->free_vals_count) {
        return object_make_null();
    }
    if (freevals_are_allocated(fun)) {
        return fun->free_vals_allocated[ix];
    } else {
        return fun->free_vals_buf[ix];
    }
}

void object_set_function_free_val(object_t obj, int ix, object_t val) {
    APE_ASSERT(object_get_type(obj) == OBJECT_FUNCTION);
    object_data_t *data = object_get_allocated_data(obj);
    APE_ASSERT(data);
    if (!data) {
        return;
    }
    function_t *fun = &data->function;
    APE_ASSERT(ix >= 0 && ix < fun->free_vals_count);
    if (ix < 0 || ix >= fun->free_vals_count) {
        return;
    }
    if (freevals_are_allocated(fun)) {
        fun->free_vals_allocated[ix] = val;
    } else {
        fun->free_vals_buf[ix] = val;
    }
}

object_t* object_get_function_free_vals(object_t obj) {
    APE_ASSERT(object_get_type(obj) == OBJECT_FUNCTION);
    object_data_t *data = object_get_allocated_data(obj);
    APE_ASSERT(data);
    if (!data) {
        return NULL;
    }
    function_t *fun = &data->function;
    if (freevals_are_allocated(fun)) {
        return fun->free_vals_allocated;
    } else {
        return fun->free_vals_buf;
    }
}

const char* object_get_error_message(object_t object) {
    APE_ASSERT(object_get_type(object) == OBJECT_ERROR);
    object_data_t *data = object_get_allocated_data(object);
    return data->error.message;
}

void object_set_error_traceback(object_t object, traceback_t *traceback) {
    APE_ASSERT(object_get_type(object) == OBJECT_ERROR);
    if (object_get_type(object) != OBJECT_ERROR) {
        return;
    }
    object_data_t *data = object_get_allocated_data(object);
    APE_ASSERT(data->error.traceback == NULL);
    data->error.traceback = traceback;
}

traceback_t* object_get_error_traceback(object_t object) {
    APE_ASSERT(object_get_type(object) == OBJECT_ERROR);
    object_data_t *data = object_get_allocated_data(object);
    return data->error.traceback;
}

bool object_set_external_data(object_t object, void *ext_data) {
    APE_ASSERT(object_get_type(object) == OBJECT_EXTERNAL);
    external_data_t* data = object_get_external_data(object);
    if (!data) {
        return false;
    }
    data->data = ext_data;
    return true;
}

bool object_set_external_copy_function(object_t object, external_data_copy_fn copy_fn) {
    APE_ASSERT(object_get_type(object) == OBJECT_EXTERNAL);
    external_data_t* data = object_get_external_data(object);
    if (!data) {
        return false;
    }
    data->data_copy_fn = copy_fn;
    return true;
}

object_t object_get_array_value_at(object_t object, int ix) {
    APE_ASSERT(object_get_type(object) == OBJECT_ARRAY);
    array(object_t)* array = object_get_allocated_array(object);
    if (ix < 0 || ix >= array_count(array)) {
        return object_make_null();
    }
    object_t *res = array_get(array, ix);
    if (!res) {
        return object_make_null();
    }
    return *res;
}

bool object_set_array_value_at(object_t object, int ix, object_t val) {
    APE_ASSERT(object_get_type(object) == OBJECT_ARRAY);
    array(object_t)* array = object_get_allocated_array(object);
    if (ix < 0 || ix >= array_count(array)) {
        return false;
    }
    return array_set(array, ix, &val);
}

bool object_add_array_value(object_t object, object_t val) {
    APE_ASSERT(object_get_type(object) == OBJECT_ARRAY);
    array(object_t)* array = object_get_allocated_array(object);
    return array_add(array, &val);
}

int object_get_array_length(object_t object) {
    APE_ASSERT(object_get_type(object) == OBJECT_ARRAY);
    array(object_t)* array = object_get_allocated_array(object);
    return array_count(array);
}

APE_INTERNAL bool object_remove_array_value_at(object_t object, int ix) {
    array(object_t)* array = object_get_allocated_array(object);
    return array_remove_at(array, ix);
}

int object_get_map_length(object_t object) {
    APE_ASSERT(object_get_type(object) == OBJECT_MAP);
    object_data_t *data = object_get_allocated_data(object);
    return valdict_count(data->map);
}

object_t object_get_map_key_at(object_t object, int ix) {
    APE_ASSERT(object_get_type(object) == OBJECT_MAP);
    object_data_t *data = object_get_allocated_data(object);
    object_t *res = valdict_get_key_at(data->map, ix);
    if (!res) {
        return object_make_null();
    }
    return *res;
}

object_t object_get_map_value_at(object_t object, int ix) {
    APE_ASSERT(object_get_type(object) == OBJECT_MAP);
    object_data_t *data = object_get_allocated_data(object);
    object_t *res = valdict_get_value_at(data->map, ix);
    if (!res) {
        return object_make_null();
    }
    return *res;
}

bool object_set_map_value_at(object_t object, int ix, object_t val) {
    APE_ASSERT(object_get_type(object) == OBJECT_MAP);
    if (ix >= object_get_map_length(object)) {
        return false;
    }
    object_data_t *data = object_get_allocated_data(object);
    return valdict_set_value_at(data->map, ix, &val);
}

object_t object_get_kv_pair_at(gcmem_t *mem, object_t object, int ix) {
    APE_ASSERT(object_get_type(object) == OBJECT_MAP);
    object_data_t *data = object_get_allocated_data(object);
    if (ix >= valdict_count(data->map)) {
        return object_make_null();
    }
    object_t key = object_get_map_key_at(object, ix);
    object_t val = object_get_map_value_at(object, ix);
    object_t res = object_make_map(mem);
    if (object_is_null(res)) {
        return object_make_null();
    }

    object_t key_obj = object_make_string(mem, "key");
    if (object_is_null(key_obj)) {
        return object_make_null();
    }
    object_set_map_value(res, key_obj, key);

    object_t val_obj = object_make_string(mem, "value");
    if (object_is_null(val_obj)) {
        return object_make_null();
    }
    object_set_map_value(res, val_obj, val);

    return res;
}

bool object_set_map_value(object_t object, object_t key, object_t val) {
    APE_ASSERT(object_get_type(object) == OBJECT_MAP);
    object_data_t *data = object_get_allocated_data(object);
    return valdict_set(data->map, &key, &val);
}

object_t object_get_map_value(object_t object, object_t key) {
    APE_ASSERT(object_get_type(object) == OBJECT_MAP);
    object_data_t *data = object_get_allocated_data(object);
    object_t *res = valdict_get(data->map, &key);
    if (!res) {
        return object_make_null();
    }
    return *res;
}

bool object_map_has_key(object_t object, object_t key) {
    APE_ASSERT(object_get_type(object) == OBJECT_MAP);
    object_data_t *data = object_get_allocated_data(object);
    object_t *res = valdict_get(data->map, &key);
    return res != NULL;
}

// INTERNAL
static object_t object_deep_copy_internal(gcmem_t *mem, object_t obj, valdict(object_t, object_t) *copies) {
    object_t *copy_ptr = valdict_get(copies, &obj);
    if (copy_ptr) {
        return *copy_ptr;
    }

    object_t copy = object_make_null()  ;

    object_type_t type = object_get_type(obj);
    switch (type) {
        case OBJECT_FREED:
        case OBJECT_ANY:
        case OBJECT_NONE: {
            APE_ASSERT(false);
            copy = object_make_null();
            break;
        }
        case OBJECT_NUMBER:
        case OBJECT_BOOL:
        case OBJECT_NULL:
        case OBJECT_NATIVE_FUNCTION: {
            copy = obj;
            break;
        }
        case OBJECT_STRING: {
            const char *str = object_get_string(obj);
            copy = object_make_string(mem, str);
            break;
        }
        case OBJECT_FUNCTION: {
            function_t *function = object_get_function(obj);
            uint8_t *bytecode_copy = NULL;
            src_pos_t *src_positions_copy = NULL;
            compilation_result_t *comp_res_copy = NULL;

            bytecode_copy = allocator_malloc(mem->alloc, sizeof(uint8_t) * function->comp_result->count);
            if (!bytecode_copy) {
                return object_make_null();
            }
            memcpy(bytecode_copy, function->comp_result->bytecode, sizeof(uint8_t) * function->comp_result->count);

            src_positions_copy = allocator_malloc(mem->alloc, sizeof(src_pos_t) * function->comp_result->count);
            if (!src_positions_copy) {
                allocator_free(mem->alloc, bytecode_copy);
                return object_make_null();
            }
            memcpy(src_positions_copy, function->comp_result->src_positions, sizeof(src_pos_t) * function->comp_result->count);

            comp_res_copy = compilation_result_make(mem->alloc, bytecode_copy, src_positions_copy, function->comp_result->count); // todo: add compilation result copy function
            if (!comp_res_copy) {
                allocator_free(mem->alloc, src_positions_copy);
                allocator_free(mem->alloc, bytecode_copy);
                return object_make_null();
            }

            copy = object_make_function(mem, object_get_function_name(obj), comp_res_copy, true,
                                        function->num_locals, function->num_args, 0);
            if (object_is_null(copy)) {
                compilation_result_destroy(comp_res_copy);
                return object_make_null();
            }

            bool ok = valdict_set(copies, &obj, &copy);
            if (!ok) {
                return object_make_null();
            }

            function_t *function_copy = object_get_function(copy);
            if (freevals_are_allocated(function)) {
                function_copy->free_vals_allocated = allocator_malloc(mem->alloc, sizeof(object_t) * function->free_vals_count);
                if (!function_copy->free_vals_allocated) {
                    return object_make_null();
                }
            }

            function_copy->free_vals_count = function->free_vals_count;
            for (int i = 0; i < function->free_vals_count; i++) {
                object_t free_val = object_get_function_free_val(obj, i);
                object_t free_val_copy = object_deep_copy_internal(mem, free_val, copies);
                if (!object_is_null(free_val) && object_is_null(free_val_copy)) {
                    return object_make_null();
                }
                object_set_function_free_val(copy, i, free_val_copy);
            }
            break;
        }
        case OBJECT_ARRAY: {
            int len = object_get_array_length(obj);
            copy = object_make_array_with_capacity(mem, len);
            if (object_is_null(copy)) {
                return object_make_null();
            }
            bool ok = valdict_set(copies, &obj, &copy);
            if (!ok) {
                return object_make_null();
            }
            for (int i = 0; i < len; i++) {
                object_t item = object_get_array_value_at(obj, i);
                object_t item_copy = object_deep_copy_internal(mem, item, copies);
                if (!object_is_null(item) && object_is_null(item_copy)) {
                    return object_make_null();
                }
                bool ok = object_add_array_value(copy, item_copy);
                if (!ok) {
                    return object_make_null();
                }
            }
            break;
        }
        case OBJECT_MAP: {
            copy = object_make_map(mem);
            if (object_is_null(copy)) {
                return object_make_null();
            }
            bool ok = valdict_set(copies, &obj, &copy);
            if (!ok) {
                return object_make_null();
            }
            for (int i = 0; i < object_get_map_length(obj); i++) {
                object_t key = object_get_map_key_at(obj, i);
                object_t val = object_get_map_value_at(obj, i);

                object_t key_copy = object_deep_copy_internal(mem, key, copies);
                if (!object_is_null(key) && object_is_null(key_copy)) {
                    return object_make_null();
                }

                object_t val_copy = object_deep_copy_internal(mem, val, copies);
                if (!object_is_null(val) && object_is_null(val_copy)) {
                    return object_make_null();
                }

                bool ok = object_set_map_value(copy, key_copy, val_copy);
                if (!ok) {
                    return object_make_null();
                }
            }
            break;
        }
        case OBJECT_EXTERNAL: {
            copy = object_copy(mem, obj);
            break;
        }
        case OBJECT_ERROR: {
            copy = obj;
            break;
        }
    }
    return copy;
}


static bool object_equals_wrapped(const object_t *a_ptr, const object_t *b_ptr) {
    object_t a = *a_ptr;
    object_t b = *b_ptr;
    return object_equals(a, b);
}

static unsigned long object_hash(object_t *obj_ptr) {
    object_t obj = *obj_ptr;
    object_type_t type = object_get_type(obj);

    switch (type) {
        case OBJECT_NUMBER: {
            double val = object_get_number(obj);
            return object_hash_double(val);
        }
        case OBJECT_BOOL: {
            bool val = object_get_bool(obj);
            return val;
        }
        case OBJECT_STRING: {
            return object_get_string_hash(obj);
        }
        default: {
            return 0;
        }
    }
}

static unsigned long object_hash_string(const char *str) { /* djb2 */
    unsigned long hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }
    return hash;
}

static unsigned long object_hash_double(double val) { /* djb2 */
    uint32_t *val_ptr = (uint32_t*)&val;
    unsigned long hash = 5381;
    hash = ((hash << 5) + hash) + val_ptr[0];
    hash = ((hash << 5) + hash) + val_ptr[1];
    return hash;
}

array(object_t)* object_get_allocated_array(object_t object) {
    APE_ASSERT(object_get_type(object) == OBJECT_ARRAY);
    object_data_t *data = object_get_allocated_data(object);
    return data->array;
}

static bool object_is_number(object_t o) {
    return (o.handle & OBJECT_PATTERN) != OBJECT_PATTERN;
}

static uint64_t get_type_tag(object_type_t type) {
    switch (type) {
        case OBJECT_NONE: return 0;
        case OBJECT_BOOL: return 1;
        case OBJECT_NULL: return 2;
        default:          return 4;
    }
}

static bool freevals_are_allocated(function_t *fun) {
    return fun->free_vals_count >= APE_ARRAY_LEN(fun->free_vals_buf);
}

static char *object_data_get_string(object_data_t *data) {
    APE_ASSERT(data->type == OBJECT_STRING);
    if (data->string.is_allocated) {
        return data->string.value_allocated;
    } else {
        return data->string.value_buf;
    }
}

static bool object_data_string_reserve_capacity(object_data_t *data, int capacity) {
    APE_ASSERT(capacity >= 0);
    
    object_string_t *string = &data->string;

    string->length = 0;
    string->hash = 0;

    if (capacity <= string->capacity) {
        return true;
    }

    if (capacity <= (OBJECT_STRING_BUF_SIZE - 1)) {
        if (string->is_allocated) {
            APE_ASSERT(false); // should never happen
            allocator_free(data->mem->alloc, string->value_allocated); // just in case
        }
        string->capacity = OBJECT_STRING_BUF_SIZE - 1;
        string->is_allocated = false;
        return true;
    }

    char *new_value = allocator_malloc(data->mem->alloc, capacity + 1);
    if (!new_value) {
        return false;
    }

    if (string->is_allocated) {
        allocator_free(data->mem->alloc, string->value_allocated);
    }

    string->value_allocated = new_value;
    string->is_allocated = true;
    string->capacity = capacity;
    return true;
}
