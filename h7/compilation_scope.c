#ifndef APE_AMALGAMATED
#include "compilation_scope.h"
#endif

compilation_scope_t *compilation_scope_make(allocator_t *alloc, compilation_scope_t *outer) {
    compilation_scope_t *scope = allocator_malloc(alloc, sizeof(compilation_scope_t));
    if (!scope) {
        return NULL;
    }
    memset(scope, 0, sizeof(compilation_scope_t));
    scope->alloc = alloc;
    scope->outer = outer;
    scope->bytecode = array_make(alloc, uint8_t);
    if (!scope->bytecode) {
        goto err;
    }
    scope->src_positions = array_make(alloc, src_pos_t);
    if (!scope->src_positions) {
        goto err;
    }
    scope->break_ip_stack = array_make(alloc, int);
    if (!scope->break_ip_stack) {
        goto err;
    }
    scope->continue_ip_stack = array_make(alloc, int);
    if (!scope->continue_ip_stack) {
        goto err;
    }
    return scope;
err:
    compilation_scope_destroy(scope);
    return NULL;
}

void compilation_scope_destroy(compilation_scope_t *scope) {
    array_destroy(scope->continue_ip_stack);
    array_destroy(scope->break_ip_stack);
    array_destroy(scope->bytecode);
    array_destroy(scope->src_positions);
    allocator_free(scope->alloc, scope);
}

compilation_result_t* compilation_scope_orphan_result(compilation_scope_t *scope) {
    compilation_result_t *res = compilation_result_make(scope->alloc,
                                                        array_data(scope->bytecode),
                                                        array_data(scope->src_positions),
                                                        array_count(scope->bytecode));
    if (!res) {
        return NULL;
    }
    array_orphan_data(scope->bytecode);
    array_orphan_data(scope->src_positions);
    return res;
}

compilation_result_t* compilation_result_make(allocator_t *alloc, uint8_t *bytecode, src_pos_t *src_positions, int count) {
    compilation_result_t *res = allocator_malloc(alloc, sizeof(compilation_result_t));
    if (!res) {
        return NULL;
    }
    memset(res, 0, sizeof(compilation_result_t));
    res->alloc = alloc;
    res->bytecode = bytecode;
    res->src_positions = src_positions;
    res->count = count;
    return res;
}

void compilation_result_destroy(compilation_result_t *res) {
    if (!res) {
        return;
    }
    allocator_free(res->alloc, res->bytecode);
    allocator_free(res->alloc, res->src_positions);
    allocator_free(res->alloc, res);
}
