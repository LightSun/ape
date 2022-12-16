#include "ape.h"

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#define APE_IMPL_VERSION_MAJOR 0
#define APE_IMPL_VERSION_MINOR 14
#define APE_IMPL_VERSION_PATCH 0

#if (APE_VERSION_MAJOR != APE_IMPL_VERSION_MAJOR)\
 || (APE_VERSION_MINOR != APE_IMPL_VERSION_MINOR)\
 || (APE_VERSION_PATCH != APE_IMPL_VERSION_PATCH)
    #error "Version mismatch"
#endif

#ifndef APE_AMALGAMATED
#include "ape.h"
#include "gc.h"
#include "compiler.h"
#include "lexer.h"
#include "parser.h"
#include "vm.h"
#include "errors.h"
#include "symbol_table.h"
#include "traceback.h"
#include "global_store.h"
#endif

typedef struct native_fn_wrapper {
    ape_native_fn fn;
    ape_t *ape;
    void *data;
} native_fn_wrapper_t;

typedef struct ape_program {
    ape_t *ape;
    compilation_result_t *comp_res;
} ape_program_t;

typedef struct ape {
    allocator_t alloc;
    gcmem_t *mem;
    ptrarray(compiled_file_t) *files;
    global_store_t *global_store;
    compiler_t *compiler;
    vm_t *vm;
    errors_t errors;
    ape_config_t config;

    allocator_t custom_allocator;

} ape_t;

static void ape_deinit(ape_t *ape);
static object_t ape_native_fn_wrapper(vm_t *vm, void *data, int argc, object_t *args);
static object_t ape_object_to_object(ape_object_t obj);
static ape_object_t object_to_ape_object(object_t obj);
static ape_object_t ape_object_make_native_function_with_name(ape_t *ape, const char *name, ape_native_fn fn, void *data);

static void reset_state(ape_t *ape);
static void set_default_config(ape_t *ape);
static char* read_file_default(void *ctx, const char *filename);
static size_t write_file_default(void* context, const char *path, const char *string, size_t string_size);
static size_t stdout_write_default(void* context, const void *data, size_t size);

static void* ape_malloc(void *ctx, size_t size);
static void ape_free(void *ctx, void *ptr);

//-----------------------------------------------------------------------------
// Ape
//-----------------------------------------------------------------------------
ape_t *ape_make(void) {
    return ape_make_ex(NULL, NULL, NULL);
}

ape_t* ape_make_ex(ape_malloc_fn malloc_fn, ape_free_fn free_fn, void *ctx) {
    allocator_t custom_alloc = allocator_make((allocator_malloc_fn)malloc_fn, (allocator_free_fn)free_fn, ctx);
    
    ape_t *ape = allocator_malloc(&custom_alloc, sizeof(ape_t));
    if (!ape) {
        return NULL;
    }

    memset(ape, 0, sizeof(ape_t));
    ape->alloc = allocator_make(ape_malloc, ape_free, ape);
    ape->custom_allocator = custom_alloc;

    set_default_config(ape);

    errors_init(&ape->errors);

    ape->mem = gcmem_make(&ape->alloc);
    if (!ape->mem) {
        goto err;
    }

    ape->files = ptrarray_make(&ape->alloc);
    if (!ape->files) {
        goto err;
    }

    ape->global_store = global_store_make(&ape->alloc, ape->mem);
    if (!ape->global_store) {
        goto err;
    }

    ape->compiler = compiler_make(&ape->alloc, &ape->config, ape->mem, &ape->errors, ape->files, ape->global_store);
    if (!ape->compiler) {
        goto err;
    }
    
    ape->vm = vm_make(&ape->alloc, &ape->config, ape->mem, &ape->errors, ape->global_store);
    if (!ape->vm) {
        goto err;
    }

    return ape;
err:
    ape_deinit(ape);
    allocator_free(&custom_alloc, ape);
    return NULL;
}

void ape_destroy(ape_t *ape) {
    if (!ape) {
        return;
    }
    ape_deinit(ape);
    allocator_t alloc = ape->alloc;
    allocator_free(&alloc, ape);
}

void ape_free_allocated(ape_t *ape, void *ptr) {
    allocator_free(&ape->alloc, ptr);
}

void ape_set_repl_mode(ape_t *ape, bool enabled) {
    ape->config.repl_mode = enabled;
}

bool ape_set_timeout(ape_t *ape, double max_execution_time_ms) {
    if (!ape_timer_platform_supported()) {
        ape->config.max_execution_time_ms = 0;
        ape->config.max_execution_time_set = false;
        return false;
    }

    if (max_execution_time_ms >= 0) {
        ape->config.max_execution_time_ms = max_execution_time_ms;
        ape->config.max_execution_time_set = true;
    } else {
        ape->config.max_execution_time_ms = 0;
        ape->config.max_execution_time_set = false;
    }
    return true;
}

void ape_set_stdout_write_function(ape_t *ape, ape_stdout_write_fn stdout_write, void *context) {
    ape->config.stdio.write.write = stdout_write;
    ape->config.stdio.write.context = context;
}

void ape_set_file_write_function(ape_t *ape, ape_write_file_fn file_write, void *context) {
    ape->config.fileio.write_file.write_file = file_write;
    ape->config.fileio.write_file.context = context;
}

void ape_set_file_read_function(ape_t *ape, ape_read_file_fn file_read, void *context) {
    ape->config.fileio.read_file.read_file = file_read;
    ape->config.fileio.read_file.context = context;
}

ape_program_t* ape_compile(ape_t *ape, const char *code) {
    ape_clear_errors(ape);

    compilation_result_t *comp_res = NULL;

    comp_res = compiler_compile(ape->compiler, code);
    if (!comp_res || errors_get_count(&ape->errors) > 0) {
        goto err;
    }

    ape_program_t *program = allocator_malloc(&ape->alloc, sizeof(ape_program_t));
    if (!program) {
        goto err;
    }
    program->ape = ape;
    program->comp_res = comp_res;
    return program;

err:
    compilation_result_destroy(comp_res);
    return NULL;
}

ape_program_t* ape_compile_file(ape_t *ape, const char *path) {
    ape_clear_errors(ape);

    compilation_result_t *comp_res = NULL;

    comp_res = compiler_compile_file(ape->compiler, path);
    if (!comp_res || errors_get_count(&ape->errors) > 0) {
        goto err;
    }

    ape_program_t *program = allocator_malloc(&ape->alloc, sizeof(ape_program_t));
    if (!program) {
        goto err;
    }

    program->ape = ape;
    program->comp_res = comp_res;
    return program;

err:
    compilation_result_destroy(comp_res);
    return NULL;
}

ape_object_t ape_execute_program(ape_t *ape, const ape_program_t *program) {
    reset_state(ape);
 
    if (ape != program->ape) {
        errors_add_error(&ape->errors, ERROR_USER, src_pos_invalid, "ape program was compiled with a different ape instance");
        return ape_object_make_null();
    }

    bool ok = vm_run(ape->vm, program->comp_res, compiler_get_constants(ape->compiler));
    if (!ok || errors_get_count(&ape->errors) > 0) {
        return ape_object_make_null();
    }

    APE_ASSERT(ape->vm->sp == 0);

    object_t res = vm_get_last_popped(ape->vm);
    if (object_get_type(res) == OBJECT_NONE) {
        return ape_object_make_null();
    }

    return object_to_ape_object(res);
}

void ape_program_destroy(ape_program_t *program) {
    if (!program) {
        return;
    }
    compilation_result_destroy(program->comp_res);
    allocator_free(&program->ape->alloc, program);
}

ape_object_t ape_execute(ape_t *ape, const char *code) {
    reset_state(ape);

    compilation_result_t *comp_res = NULL;

    comp_res = compiler_compile(ape->compiler, code);
    if (!comp_res || errors_get_count(&ape->errors) > 0) {
        goto err;
    }

    bool ok = vm_run(ape->vm, comp_res, compiler_get_constants(ape->compiler));
    if (!ok || errors_get_count(&ape->errors) > 0) {
        goto err;
    }

    APE_ASSERT(ape->vm->sp == 0);

    object_t res = vm_get_last_popped(ape->vm);
    if (object_get_type(res) == OBJECT_NONE) {
        goto err;
    }

    compilation_result_destroy(comp_res);

    return object_to_ape_object(res);

err:
    compilation_result_destroy(comp_res);
    return ape_object_make_null();
}

ape_object_t ape_execute_file(ape_t *ape, const char *path) {
    reset_state(ape);

    compilation_result_t *comp_res = NULL;

    comp_res = compiler_compile_file(ape->compiler, path);
    if (!comp_res || errors_get_count(&ape->errors) > 0) {
        goto err;
    }

    bool ok = vm_run(ape->vm, comp_res, compiler_get_constants(ape->compiler));
    if (!ok || errors_get_count(&ape->errors) > 0) {
        goto err;
    }

    APE_ASSERT(ape->vm->sp == 0);

    object_t res = vm_get_last_popped(ape->vm);
    if (object_get_type(res) == OBJECT_NONE) {
        goto err;
    }

    compilation_result_destroy(comp_res);

    return object_to_ape_object(res);

err:
    compilation_result_destroy(comp_res);
    return ape_object_make_null();
}

ape_object_t ape_call(ape_t *ape, const char *function_name, int argc, ape_object_t *args) {
    reset_state(ape);

    object_t callee = ape_object_to_object(ape_get_object(ape, function_name));
    if (object_get_type(callee) == OBJECT_NULL) {
        return ape_object_make_null();
    }
    object_t res = vm_call(ape->vm, compiler_get_constants(ape->compiler), callee, argc, (object_t*)args);
    if (errors_get_count(&ape->errors) > 0) {
        return ape_object_make_null();
    }
    return object_to_ape_object(res);
}

bool ape_has_errors(const ape_t *ape) {
    return ape_errors_count(ape) > 0;
}

int ape_errors_count(const ape_t *ape) {
    return errors_get_count(&ape->errors);
}

void ape_clear_errors(ape_t *ape) {
    errors_clear(&ape->errors);
}

const ape_error_t* ape_get_error(const ape_t *ape, int index) {
    return (const ape_error_t*)errors_getc(&ape->errors, index);
}

bool ape_set_native_function(ape_t *ape, const char *name, ape_native_fn fn, void *data) {
    ape_object_t obj = ape_object_make_native_function_with_name(ape, name, fn, data);
    if (ape_object_is_null(obj)) {
        return false;
    }
    return ape_set_global_constant(ape, name, obj);
}

bool ape_set_global_constant(ape_t *ape, const char *name, ape_object_t obj) {
    return global_store_set(ape->global_store, name, ape_object_to_object(obj));
}

ape_object_t ape_get_object(ape_t *ape, const char *name) {
    symbol_table_t *st = compiler_get_symbol_table(ape->compiler);
    const symbol_t *symbol = symbol_table_resolve(st, name);
    if (!symbol) {
        errors_add_errorf(&ape->errors, ERROR_USER, src_pos_invalid, "Symbol \"%s\" is not defined", name);
        return ape_object_make_null();
    }
    object_t res = object_make_null();
    if (symbol->type == SYMBOL_MODULE_GLOBAL) {
        res = vm_get_global(ape->vm, symbol->index);
    } else if (symbol->type == SYMBOL_APE_GLOBAL) {
        bool ok = false;
        res = global_store_get_object_at(ape->global_store, symbol->index, &ok);
        if (!ok) {
            errors_add_errorf(&ape->errors, ERROR_USER, src_pos_invalid, "Failed to get global object at %d", symbol->index);
            return ape_object_make_null();
        }
    } else {
        errors_add_errorf(&ape->errors, ERROR_USER, src_pos_invalid, "Value associated with symbol \"%s\" could not be loaded", name);
        return ape_object_make_null();
    }
    return object_to_ape_object(res);
}

bool ape_check_args(ape_t *ape, bool generate_error, int argc, ape_object_t *args, int expected_argc, int *expected_types) {
    if (argc != expected_argc) {
        if (generate_error) {
            ape_set_runtime_errorf(ape, "Invalid number or arguments, got %d instead of %d", argc, expected_argc);
        }
        return false;
    }
    
    for(int i = 0; i < argc; i++) {
        ape_object_t arg = args[i];
        ape_object_type_t type = ape_object_get_type(arg);
        ape_object_type_t expected_type = expected_types[i];
        if (!(type & expected_type)) {
            if (generate_error) {
                const char *type_str = ape_object_get_type_name(type);
                const char *expected_type_str = ape_object_get_type_name(expected_type);
                ape_set_runtime_errorf(ape, "Invalid argument type, got %s, expected %s", type_str, expected_type_str);
            }
            return false;
        }
    }
    return true;
}

//-----------------------------------------------------------------------------
// Ape object
//-----------------------------------------------------------------------------

ape_object_t ape_object_make_number(double val) {
    return object_to_ape_object(object_make_number(val));
}

ape_object_t ape_object_make_bool(bool val) {
    return object_to_ape_object(object_make_bool(val));
}

ape_object_t ape_object_make_string(ape_t *ape, const char *str) {
    return object_to_ape_object(object_make_string(ape->mem, str));
}

ape_object_t ape_object_make_stringf(ape_t *ape, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int to_write = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    va_start(args, fmt);
    object_t res = object_make_string_with_capacity(ape->mem, to_write);
    if (object_is_null(res)) {
        return ape_object_make_null();
    }
    char *res_buf = object_get_mutable_string(res);
    int written = vsprintf(res_buf, fmt, args);
    (void)written;
    APE_ASSERT(written == to_write);
    va_end(args);
    object_set_string_length(res, to_write);
    return object_to_ape_object(res);
}

ape_object_t ape_object_make_null() {
    return object_to_ape_object(object_make_null());
}

ape_object_t ape_object_make_array(ape_t *ape) {
    return object_to_ape_object(object_make_array(ape->mem));
}

ape_object_t ape_object_make_map(ape_t *ape) {
    return object_to_ape_object(object_make_map(ape->mem));
}

ape_object_t ape_object_make_native_function(ape_t *ape, ape_native_fn fn, void *data) {
    return ape_object_make_native_function_with_name(ape, "", fn, data);
}

ape_object_t ape_object_make_error(ape_t *ape, const char *msg) {
    return object_to_ape_object(object_make_error(ape->mem, msg));
}

ape_object_t ape_object_make_errorf(ape_t *ape, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int to_write = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    va_start(args, fmt);
    char *res = (char*)allocator_malloc(&ape->alloc, to_write + 1);
    if (!res) {
        return ape_object_make_null();
    }
    int written = vsprintf(res, fmt, args);
    (void)written;
    APE_ASSERT(written == to_write);
    va_end(args);
    return object_to_ape_object(object_make_error_no_copy(ape->mem, res));
}

ape_object_t ape_object_make_external(ape_t *ape, void *data) {
    object_t res = object_make_external(ape->mem, data);
    return object_to_ape_object(res);
}

char *ape_object_serialize(ape_t *ape, ape_object_t obj) {
    return object_serialize(&ape->alloc, ape_object_to_object(obj));
}

bool ape_object_disable_gc(ape_object_t ape_obj) {
    object_t obj = ape_object_to_object(ape_obj);
    return gc_disable_on_object(obj);
}

void ape_object_enable_gc(ape_object_t ape_obj) {
    object_t obj = ape_object_to_object(ape_obj);
    gc_enable_on_object(obj);
}

bool ape_object_equals(ape_object_t ape_a, ape_object_t ape_b){
    object_t a = ape_object_to_object(ape_a);
    object_t b = ape_object_to_object(ape_b);
    return object_equals(a, b);
}

bool ape_object_is_null(ape_object_t obj) {
    return ape_object_get_type(obj) == APE_OBJECT_NULL;
}

ape_object_t ape_object_copy(ape_object_t ape_obj) {
    object_t obj = ape_object_to_object(ape_obj);
    gcmem_t *mem = object_get_mem(obj);
    object_t res = object_copy(mem, obj);
    return object_to_ape_object(res);
}

ape_object_t ape_object_deep_copy(ape_object_t ape_obj) {
    object_t obj = ape_object_to_object(ape_obj);
    gcmem_t *mem = object_get_mem(obj);
    object_t res = object_deep_copy(mem, obj);
    return object_to_ape_object(res);
}

void ape_set_runtime_error(ape_t *ape, const char *message) {
    errors_add_error(&ape->errors, ERROR_RUNTIME, src_pos_invalid, message);
}

void ape_set_runtime_errorf(ape_t *ape, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int to_write = vsnprintf(NULL, 0, fmt, args);
    (void)to_write;
    va_end(args);
    va_start(args, fmt);
    char res[ERROR_MESSAGE_MAX_LENGTH];
    int written = vsnprintf(res, ERROR_MESSAGE_MAX_LENGTH, fmt, args);
    (void)written;
    APE_ASSERT(to_write == written);
    va_end(args);
    errors_add_error(&ape->errors, ERROR_RUNTIME, src_pos_invalid, res);
}

ape_object_type_t ape_object_get_type(ape_object_t ape_obj) {
    object_t obj = ape_object_to_object(ape_obj);
    switch (object_get_type(obj)) {
        case OBJECT_NONE:            return APE_OBJECT_NONE;
        case OBJECT_ERROR:           return APE_OBJECT_ERROR;
        case OBJECT_NUMBER:          return APE_OBJECT_NUMBER;
        case OBJECT_BOOL:            return APE_OBJECT_BOOL;
        case OBJECT_STRING:          return APE_OBJECT_STRING;
        case OBJECT_NULL:            return APE_OBJECT_NULL;
        case OBJECT_NATIVE_FUNCTION: return APE_OBJECT_NATIVE_FUNCTION;
        case OBJECT_ARRAY:           return APE_OBJECT_ARRAY;
        case OBJECT_MAP:             return APE_OBJECT_MAP;
        case OBJECT_FUNCTION:        return APE_OBJECT_FUNCTION;
        case OBJECT_EXTERNAL:        return APE_OBJECT_EXTERNAL;
        case OBJECT_FREED:           return APE_OBJECT_FREED;
        case OBJECT_ANY:             return APE_OBJECT_ANY;
        default:                     return APE_OBJECT_NONE;
    }
}

const char* ape_object_get_type_string(ape_object_t obj) {
    return ape_object_get_type_name(ape_object_get_type(obj));
}

const char* ape_object_get_type_name(ape_object_type_t type) {
    switch (type) {
        case APE_OBJECT_NONE:            return "NONE";
        case APE_OBJECT_ERROR:           return "ERROR";
        case APE_OBJECT_NUMBER:          return "NUMBER";
        case APE_OBJECT_BOOL:            return "BOOL";
        case APE_OBJECT_STRING:          return "STRING";
        case APE_OBJECT_NULL:            return "NULL";
        case APE_OBJECT_NATIVE_FUNCTION: return "NATIVE_FUNCTION";
        case APE_OBJECT_ARRAY:           return "ARRAY";
        case APE_OBJECT_MAP:             return "MAP";
        case APE_OBJECT_FUNCTION:        return "FUNCTION";
        case APE_OBJECT_EXTERNAL:        return "EXTERNAL";
        case APE_OBJECT_FREED:           return "FREED";
        case APE_OBJECT_ANY:             return "ANY";
        default:                         return "NONE";
    }
}

double ape_object_get_number(ape_object_t obj) {
    return object_get_number(ape_object_to_object(obj));
}

bool ape_object_get_bool(ape_object_t obj) {
    return object_get_bool(ape_object_to_object(obj));
}

const char *ape_object_get_string(ape_object_t obj) {
    return object_get_string(ape_object_to_object(obj));
}

const char *ape_object_get_error_message(ape_object_t obj) {
    return object_get_error_message(ape_object_to_object(obj));
}

const ape_traceback_t* ape_object_get_error_traceback(ape_object_t ape_obj) {
    object_t obj = ape_object_to_object(ape_obj);
    return (const ape_traceback_t*)object_get_error_traceback(obj);
}

bool ape_object_set_external_destroy_function(ape_object_t object, ape_data_destroy_fn destroy_fn) {
    return object_set_external_destroy_function(ape_object_to_object(object), (external_data_destroy_fn)destroy_fn);
}

bool ape_object_set_external_copy_function(ape_object_t object, ape_data_copy_fn copy_fn) {
    return object_set_external_copy_function(ape_object_to_object(object), (external_data_copy_fn)copy_fn);
}

//-----------------------------------------------------------------------------
// Ape object array
//-----------------------------------------------------------------------------

int ape_object_get_array_length(ape_object_t obj) {
    return object_get_array_length(ape_object_to_object(obj));
}

ape_object_t ape_object_get_array_value(ape_object_t obj, int ix) {
    object_t res = object_get_array_value_at(ape_object_to_object(obj), ix);
    return object_to_ape_object(res);
}

const char* ape_object_get_array_string(ape_object_t obj, int ix) {
    ape_object_t object = ape_object_get_array_value(obj, ix);
    if (ape_object_get_type(object) != APE_OBJECT_STRING) {
        return NULL;
    }
    return ape_object_get_string(object);
}

double ape_object_get_array_number(ape_object_t obj, int ix) {
    ape_object_t object = ape_object_get_array_value(obj, ix);
    if (ape_object_get_type(object) != APE_OBJECT_NUMBER) {
        return 0;
    }
    return ape_object_get_number(object);
}

bool ape_object_get_array_bool(ape_object_t obj, int ix) {
    ape_object_t object = ape_object_get_array_value(obj, ix);
    if (ape_object_get_type(object) != APE_OBJECT_BOOL) {
        return 0;
    }
    return ape_object_get_bool(object);
}

bool ape_object_set_array_value(ape_object_t ape_obj, int ix, ape_object_t ape_value) {
    object_t obj = ape_object_to_object(ape_obj);
    object_t value = ape_object_to_object(ape_value);
    return object_set_array_value_at(obj, ix, value);
}

bool ape_object_set_array_string(ape_object_t obj, int ix, const char *string) {
    gcmem_t *mem = object_get_mem(ape_object_to_object(obj));
    if (!mem) {
        return false;
    }
    object_t new_value = object_make_string(mem, string);
    return ape_object_set_array_value(obj, ix, object_to_ape_object(new_value));
}

bool ape_object_set_array_number(ape_object_t obj, int ix, double number) {
    object_t new_value = object_make_number(number);
    return ape_object_set_array_value(obj, ix, object_to_ape_object(new_value));
}

bool ape_object_set_array_bool(ape_object_t obj, int ix, bool value) {
    object_t new_value = object_make_bool(value);
    return ape_object_set_array_value(obj, ix, object_to_ape_object(new_value));
}

bool ape_object_add_array_value(ape_object_t ape_obj, ape_object_t ape_value) {
    object_t obj = ape_object_to_object(ape_obj);
    object_t value = ape_object_to_object(ape_value);
    return object_add_array_value(obj, value);
}

bool ape_object_add_array_string(ape_object_t obj, const char *string) {
    gcmem_t *mem = object_get_mem(ape_object_to_object(obj));
    if (!mem) {
        return false;
    }
    object_t new_value = object_make_string(mem, string);
    return ape_object_add_array_value(obj, object_to_ape_object(new_value));
}

bool ape_object_add_array_number(ape_object_t obj, double number) {
    object_t new_value = object_make_number(number);
    return ape_object_add_array_value(obj, object_to_ape_object(new_value));
}

bool ape_object_add_array_bool(ape_object_t obj, bool value) {
    object_t new_value = object_make_bool(value);
    return ape_object_add_array_value(obj, object_to_ape_object(new_value));
}

//-----------------------------------------------------------------------------
// Ape object map
//-----------------------------------------------------------------------------

int ape_object_get_map_length(ape_object_t obj) {
    return object_get_map_length(ape_object_to_object(obj));
}

ape_object_t ape_object_get_map_key_at(ape_object_t ape_obj, int ix) {
    object_t obj = ape_object_to_object(ape_obj);
    return object_to_ape_object(object_get_map_key_at(obj, ix));
}

ape_object_t ape_object_get_map_value_at(ape_object_t ape_obj, int ix) {
    object_t obj = ape_object_to_object(ape_obj);
    object_t res = object_get_map_value_at(obj, ix);
    return object_to_ape_object(res);
}

bool ape_object_set_map_value_at(ape_object_t ape_obj, int ix, ape_object_t ape_val) {
    object_t obj = ape_object_to_object(ape_obj);
    object_t val = ape_object_to_object(ape_val);
    return object_set_map_value_at(obj, ix, val);
}

bool ape_object_set_map_value_with_value_key(ape_object_t obj, ape_object_t key, ape_object_t val) {
    return object_set_map_value(ape_object_to_object(obj), ape_object_to_object(key), ape_object_to_object(val));
}

bool ape_object_set_map_value(ape_object_t obj, const char *key, ape_object_t value) {
    gcmem_t *mem = object_get_mem(ape_object_to_object(obj));
    if (!mem) {
        return false;
    }
    object_t key_object = object_make_string(mem, key);
    if (object_is_null(key_object)) {
        return false;
    }
    return ape_object_set_map_value_with_value_key(obj, object_to_ape_object(key_object), value);
}

bool ape_object_set_map_string(ape_object_t obj, const char *key, const char *string) {
    gcmem_t *mem = object_get_mem(ape_object_to_object(obj));
    if (!mem) {
        return false;
    }
    object_t string_object = object_make_string(mem, string);
    if (object_is_null(string_object)) {
        return false;
    }
    return ape_object_set_map_value(obj, key, object_to_ape_object(string_object));
}

bool ape_object_set_map_number(ape_object_t obj, const char *key, double number) {
    object_t number_object = object_make_number(number);
    return ape_object_set_map_value(obj, key, object_to_ape_object(number_object));
}

bool ape_object_set_map_bool(ape_object_t obj, const char *key, bool value) {
    object_t bool_object = object_make_bool(value);
    return ape_object_set_map_value(obj, key, object_to_ape_object(bool_object));
}

ape_object_t ape_object_get_map_value_with_value_key(ape_object_t obj, ape_object_t key) {
    return object_to_ape_object(object_get_map_value(ape_object_to_object(obj), ape_object_to_object(key)));
}

ape_object_t ape_object_get_map_value(ape_object_t object, const char *key) {
    gcmem_t *mem = object_get_mem(ape_object_to_object(object));
    if (!mem) {
        return ape_object_make_null();
    }
    object_t key_object = object_make_string(mem, key);
    if (object_is_null(key_object)) {
        return ape_object_make_null();
    }
    ape_object_t res = ape_object_get_map_value_with_value_key(object, object_to_ape_object(key_object));
    return res;
}

const char* ape_object_get_map_string(ape_object_t object, const char *key) {
    ape_object_t res = ape_object_get_map_value(object, key);
    return ape_object_get_string(res);
}

double ape_object_get_map_number(ape_object_t object, const char *key) {
    ape_object_t res = ape_object_get_map_value(object, key);
    return ape_object_get_number(res);
}

bool ape_object_get_map_bool(ape_object_t object, const char *key) {
    ape_object_t res = ape_object_get_map_value(object, key);
    return ape_object_get_bool(res);
}

bool ape_object_map_has_key(ape_object_t ape_object, const char *key) {
    object_t object = ape_object_to_object(ape_object);
    gcmem_t *mem = object_get_mem(object);
    if (!mem) {
        return false;
    }
    object_t key_object = object_make_string(mem, key);
    if (object_is_null(key_object)) {
        return false;
    }
    return object_map_has_key(object, key_object);
}

//-----------------------------------------------------------------------------
// Ape error
//-----------------------------------------------------------------------------

const char* ape_error_get_message(const ape_error_t *ape_error) {
    const error_t *error = (const error_t*)ape_error;
    return error->message;
}

const char* ape_error_get_filepath(const ape_error_t *ape_error) {
    const error_t *error = (const error_t*)ape_error;
    if (!error->pos.file) {
        return NULL;
    }
    return error->pos.file->path;
}

const char* ape_error_get_line(const ape_error_t *ape_error) {
    const error_t *error = (const error_t*)ape_error;
    if (!error->pos.file) {
        return NULL;
    }
    ptrarray(char*) *lines = error->pos.file->lines;
    if (error->pos.line >= ptrarray_count(lines)) {
        return NULL;
    }
    const char *line = ptrarray_get(lines, error->pos.line);
    return line;
}

int ape_error_get_line_number(const ape_error_t *ape_error) {
    const error_t *error = (const error_t*)ape_error;
    if (error->pos.line < 0) {
        return -1;
    }
    return error->pos.line + 1;
}

int ape_error_get_column_number(const ape_error_t *ape_error) {
    const error_t *error = (const error_t*)ape_error;
    if (error->pos.column < 0) {
        return -1;
    }
    return error->pos.column + 1;
}

ape_error_type_t ape_error_get_type(const ape_error_t *ape_error) {
    const error_t *error = (const error_t*)ape_error;
    switch (error->type) {
        case ERROR_NONE:        return APE_ERROR_NONE;
        case ERROR_PARSING:     return APE_ERROR_PARSING;
        case ERROR_COMPILATION: return APE_ERROR_COMPILATION;
        case ERROR_RUNTIME:     return APE_ERROR_RUNTIME;
        case ERROR_TIMEOUT:     return APE_ERROR_TIMEOUT;
        case ERROR_ALLOCATION:  return APE_ERROR_ALLOCATION;
        case ERROR_USER:        return APE_ERROR_USER;
        default:                return APE_ERROR_NONE;
    }
}

const char* ape_error_get_type_string(const ape_error_t *error) {
    return ape_error_type_to_string(ape_error_get_type(error));
}

const char* ape_error_type_to_string(ape_error_type_t type) {
    switch (type) {
        case APE_ERROR_PARSING:     return "PARSING";
        case APE_ERROR_COMPILATION: return "COMPILATION";
        case APE_ERROR_RUNTIME:     return "RUNTIME";
        case APE_ERROR_TIMEOUT:     return "TIMEOUT";
        case APE_ERROR_ALLOCATION:  return "ALLOCATION";
        case APE_ERROR_USER:        return "USER";
        default:                    return "NONE";
    }
}

char* ape_error_serialize(ape_t *ape, const ape_error_t *err) {
    const char *type_str = ape_error_get_type_string(err);
    const char *filename = ape_error_get_filepath(err);
    const char *line = ape_error_get_line(err);
    int line_num = ape_error_get_line_number(err);
    int col_num = ape_error_get_column_number(err);
    strbuf_t *buf = strbuf_make(&ape->alloc);
    if (!buf) {
        return NULL;
    }
    if (line) {
        strbuf_append(buf, line);
        strbuf_append(buf, "\n");
        if (col_num >= 0) {
            for (int j = 0; j < (col_num - 1); j++) {
                strbuf_append(buf, " ");
            }
            strbuf_append(buf, "^\n");
        }
    }
    strbuf_appendf(buf, "%s ERROR in \"%s\" on %d:%d: %s\n", type_str,
           filename, line_num, col_num, ape_error_get_message(err));
    const ape_traceback_t *traceback = ape_error_get_traceback(err);
    if (traceback) {
        strbuf_appendf(buf, "Traceback:\n");
        traceback_to_string((const traceback_t*)ape_error_get_traceback(err), buf);
    }
    if (strbuf_failed(buf)) {
        strbuf_destroy(buf);
        return NULL;
    }
    return strbuf_get_string_and_destroy(buf);
}

const ape_traceback_t* ape_error_get_traceback(const ape_error_t *ape_error) {
    const error_t *error = (const error_t*)ape_error;
    return (const ape_traceback_t*)error->traceback;
}

//-----------------------------------------------------------------------------
// Ape traceback
//-----------------------------------------------------------------------------

int ape_traceback_get_depth(const ape_traceback_t *ape_traceback) {
    const traceback_t *traceback = (const traceback_t*)ape_traceback;
    return array_count(traceback->items);
}

const char* ape_traceback_get_filepath(const ape_traceback_t *ape_traceback, int depth) {
    const traceback_t *traceback = (const traceback_t*)ape_traceback;
    traceback_item_t *item = array_get(traceback->items, depth);
    if (!item) {
        return NULL;
    }
    return traceback_item_get_filepath(item);
}

const char* ape_traceback_get_line(const ape_traceback_t *ape_traceback, int depth) {
    const traceback_t *traceback = (const traceback_t*)ape_traceback;
    traceback_item_t *item = array_get(traceback->items, depth);
    if (!item) {
        return NULL;
    }
    return traceback_item_get_line(item);
}

int ape_traceback_get_line_number(const ape_traceback_t *ape_traceback, int depth) {
    const traceback_t *traceback = (const traceback_t*)ape_traceback;
    traceback_item_t *item = array_get(traceback->items, depth);
    if (!item) {
        return -1;
    }
    return item->pos.line;
}

int ape_traceback_get_column_number(const ape_traceback_t *ape_traceback, int depth) {
    const traceback_t *traceback = (const traceback_t*)ape_traceback;
    traceback_item_t *item = array_get(traceback->items, depth);
    if (!item) {
        return -1;
    }
    return item->pos.column;
}

const char* ape_traceback_get_function_name(const ape_traceback_t *ape_traceback, int depth) {
    const traceback_t *traceback = (const traceback_t*)ape_traceback;
    traceback_item_t *item = array_get(traceback->items, depth);
    if (!item) {
        return "";
    }
    return item->function_name;
}

//-----------------------------------------------------------------------------
// Ape internal
//-----------------------------------------------------------------------------
static void ape_deinit(ape_t *ape) {
    vm_destroy(ape->vm);
    compiler_destroy(ape->compiler);
    global_store_destroy(ape->global_store);
    gcmem_destroy(ape->mem);
    ptrarray_destroy_with_items(ape->files, compiled_file_destroy);
    errors_deinit(&ape->errors);
}

static object_t ape_native_fn_wrapper(vm_t *vm, void *data, int argc, object_t *args) {
    (void)vm;
    native_fn_wrapper_t *wrapper = (native_fn_wrapper_t*)data;
    APE_ASSERT(vm == wrapper->ape->vm);
    ape_object_t res = wrapper->fn(wrapper->ape, wrapper->data, argc, (ape_object_t*)args);
    if (ape_has_errors(wrapper->ape)) {
        return object_make_null();
    }
    return ape_object_to_object(res);
}

static object_t ape_object_to_object(ape_object_t obj) {
    return (object_t){ .handle = obj._internal };
}

static ape_object_t object_to_ape_object(object_t obj) {
    return (ape_object_t){ ._internal = obj.handle };
}

static ape_object_t ape_object_make_native_function_with_name(ape_t *ape, const char *name, ape_native_fn fn, void *data) {
    native_fn_wrapper_t wrapper;
    memset(&wrapper, 0, sizeof(native_fn_wrapper_t));
    wrapper.fn = fn;
    wrapper.ape = ape;
    wrapper.data = data;
    object_t wrapper_native_function = object_make_native_function(ape->mem, name, ape_native_fn_wrapper, &wrapper, sizeof(wrapper));
    if (object_is_null(wrapper_native_function)) {
        return ape_object_make_null();
    }
    return object_to_ape_object(wrapper_native_function);
}

static void reset_state(ape_t *ape) {
    ape_clear_errors(ape);
    vm_reset(ape->vm);
}

static void set_default_config(ape_t *ape) {
    memset(&ape->config, 0, sizeof(ape_config_t));
    ape_set_repl_mode(ape, false);
    ape_set_timeout(ape, -1);
    ape_set_file_read_function(ape, read_file_default, ape);
    ape_set_file_write_function(ape, write_file_default, ape);
    ape_set_stdout_write_function(ape, stdout_write_default, ape);
}

static char* read_file_default(void *ctx, const char *filename){
    ape_t *ape = (ape_t*)ctx;
    FILE *fp = fopen(filename, "r");
    size_t size_to_read = 0;
    size_t size_read = 0;
    long pos;
    char *file_contents;
    if (!fp) {
        return NULL;
    }
    fseek(fp, 0L, SEEK_END);
    pos = ftell(fp);
    if (pos < 0) {
        fclose(fp);
        return NULL;
    }
    size_to_read = pos;
    rewind(fp);
    file_contents = (char*)allocator_malloc(&ape->alloc, sizeof(char) * (size_to_read + 1));
    if (!file_contents) {
        fclose(fp);
        return NULL;
    }
    size_read = fread(file_contents, 1, size_to_read, fp);
    if (ferror(fp)) {
        fclose(fp);
        free(file_contents);
        return NULL;
    }
    fclose(fp);
    file_contents[size_read] = '\0';
    return file_contents;
}

static size_t write_file_default(void* ctx, const char *path, const char *string, size_t string_size) {
    (void)ctx;
    FILE *fp = fopen(path, "w");
    if (!fp) {
        return 0;
    }
    size_t written = fwrite(string, 1, string_size, fp);
    fclose(fp);
    return written;
}

static size_t stdout_write_default(void* ctx, const void *data, size_t size) {
    (void)ctx;
    return fwrite(data, 1, size, stdout);
}

static void* ape_malloc(void *ctx, size_t size) {
    ape_t *ape = (ape_t*)ctx;
    void *res = allocator_malloc(&ape->custom_allocator, size);
    if (!res) {
        errors_add_error(&ape->errors, ERROR_ALLOCATION, src_pos_invalid, "Allocation failed");
    }
    return res;
}

static void ape_free(void *ctx, void *ptr) {
    ape_t *ape = (ape_t*)ctx;
    allocator_free(&ape->custom_allocator, ptr);
}
