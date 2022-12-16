#include <stdlib.h>
#include <string.h>

#ifndef APE_AMALGAMATED
#include "ast.h"
#include "common.h"
#endif

static expression_t* expression_make(allocator_t *alloc, expression_type_t type);
static statement_t* statement_make(allocator_t *alloc, statement_type_t type);

expression_t* expression_make_ident(allocator_t *alloc, ident_t *ident) {
    expression_t *res = expression_make(alloc, EXPRESSION_IDENT);
    if (!res) {
        return NULL;
    }
    res->ident = ident;
    return res;
}

expression_t* expression_make_number_literal(allocator_t *alloc, double val) {
    expression_t *res = expression_make(alloc, EXPRESSION_NUMBER_LITERAL);
    if (!res) {
        return NULL;
    }
    res->number_literal = val;
    return res;
}

expression_t* expression_make_bool_literal(allocator_t *alloc, bool val) {
    expression_t *res = expression_make(alloc, EXPRESSION_BOOL_LITERAL);
    if (!res) {
        return NULL;
    }
    res->bool_literal = val;
    return res;
}

expression_t* expression_make_string_literal(allocator_t *alloc, char *value) {
    expression_t *res = expression_make(alloc, EXPRESSION_STRING_LITERAL);
    if (!res) {
        return NULL;
    }
    res->string_literal = value;
    return res;
}

expression_t* expression_make_null_literal(allocator_t *alloc) {
    expression_t *res = expression_make(alloc, EXPRESSION_NULL_LITERAL);
    if (!res) {
        return NULL;
    }
    return res;
}

expression_t* expression_make_array_literal(allocator_t *alloc, ptrarray(expression_t) *values) {
    expression_t *res = expression_make(alloc, EXPRESSION_ARRAY_LITERAL);
    if (!res) {
        return NULL;
    }
    res->array = values;
    return res;
}

expression_t* expression_make_map_literal(allocator_t *alloc, ptrarray(expression_t) *keys, ptrarray(expression_t) *values) {
    expression_t *res = expression_make(alloc, EXPRESSION_MAP_LITERAL);
    if (!res) {
        return NULL;
    }
    res->map.keys = keys;
    res->map.values = values;
    return res;
}

expression_t* expression_make_prefix(allocator_t *alloc, operator_t op, expression_t *right) {
    expression_t *res = expression_make(alloc, EXPRESSION_PREFIX);
    if (!res) {
        return NULL;
    }
    res->prefix.op = op;
    res->prefix.right = right;
    return res;
}

expression_t* expression_make_infix(allocator_t *alloc, operator_t op, expression_t *left, expression_t *right) {
    expression_t *res = expression_make(alloc, EXPRESSION_INFIX);
    if (!res) {
        return NULL;
    }
    res->infix.op = op;
    res->infix.left = left;
    res->infix.right = right;
    return res;
}

expression_t* expression_make_fn_literal(allocator_t *alloc, ptrarray(ident_t) *params, code_block_t *body) {
    expression_t *res = expression_make(alloc, EXPRESSION_FUNCTION_LITERAL);
    if (!res) {
        return NULL;
    }
    res->fn_literal.name = NULL;
    res->fn_literal.params = params;
    res->fn_literal.body = body;
    return res;
}

expression_t* expression_make_call(allocator_t *alloc, expression_t *function, ptrarray(expression_t) *args) {
    expression_t *res = expression_make(alloc, EXPRESSION_CALL);
    if (!res) {
        return NULL;
    }
    res->call_expr.function = function;
    res->call_expr.args = args;
    return res;
}

expression_t* expression_make_index(allocator_t *alloc, expression_t *left, expression_t *index) {
    expression_t *res = expression_make(alloc, EXPRESSION_INDEX);
    if (!res) {
        return NULL;
    }
    res->index_expr.left = left;
    res->index_expr.index = index;
    return res;
}

expression_t* expression_make_assign(allocator_t *alloc, expression_t *dest, expression_t *source, bool is_postfix) {
    expression_t *res = expression_make(alloc, EXPRESSION_ASSIGN);
    if (!res) {
        return NULL;
    }
    res->assign.dest = dest;
    res->assign.source = source;
    res->assign.is_postfix = is_postfix;
    return res;
}

expression_t* expression_make_logical(allocator_t *alloc, operator_t op, expression_t *left, expression_t *right) {
    expression_t *res = expression_make(alloc, EXPRESSION_LOGICAL);
    if (!res) {
        return NULL;
    }
    res->logical.op = op;
    res->logical.left = left;
    res->logical.right = right;
    return res;
}

expression_t* expression_make_ternary(allocator_t *alloc, expression_t *test, expression_t *if_true, expression_t *if_false) {
    expression_t *res = expression_make(alloc, EXPRESSION_TERNARY);
    if (!res) {
        return NULL;
    }
    res->ternary.test = test;
    res->ternary.if_true = if_true;
    res->ternary.if_false = if_false;
    return res;
}

void expression_destroy(expression_t *expr) {
    if (!expr) {
        return;
    }

    switch (expr->type) {
        case EXPRESSION_NONE: {
            APE_ASSERT(false);
            break;
        }
        case EXPRESSION_IDENT: {
            ident_destroy(expr->ident);
            break;
        }
        case EXPRESSION_NUMBER_LITERAL:
        case EXPRESSION_BOOL_LITERAL: {
            break;
        }
        case EXPRESSION_STRING_LITERAL: {
            allocator_free(expr->alloc, expr->string_literal);
            break;
        }
        case EXPRESSION_NULL_LITERAL: {
            break;
        }
        case EXPRESSION_ARRAY_LITERAL: {
            ptrarray_destroy_with_items(expr->array, expression_destroy);
            break;
        }
        case EXPRESSION_MAP_LITERAL: {
            ptrarray_destroy_with_items(expr->map.keys, expression_destroy);
            ptrarray_destroy_with_items(expr->map.values, expression_destroy);
            break;
        }
        case EXPRESSION_PREFIX: {
            expression_destroy(expr->prefix.right);
            break;
        }
        case EXPRESSION_INFIX: {
            expression_destroy(expr->infix.left);
            expression_destroy(expr->infix.right);
            break;
        }
        case EXPRESSION_FUNCTION_LITERAL: {
            fn_literal_t *fn = &expr->fn_literal;
            allocator_free(expr->alloc, fn->name);
            ptrarray_destroy_with_items(fn->params, ident_destroy);
            code_block_destroy(fn->body);
            break;
        }
        case EXPRESSION_CALL: {
            ptrarray_destroy_with_items(expr->call_expr.args, expression_destroy);
            expression_destroy(expr->call_expr.function);
            break;
        }
        case EXPRESSION_INDEX: {
            expression_destroy(expr->index_expr.left);
            expression_destroy(expr->index_expr.index);
            break;
        }
        case EXPRESSION_ASSIGN: {
            expression_destroy(expr->assign.dest);
            expression_destroy(expr->assign.source);
            break;
        }
        case EXPRESSION_LOGICAL: {
            expression_destroy(expr->logical.left);
            expression_destroy(expr->logical.right);
            break;
        }
        case EXPRESSION_TERNARY: {
            expression_destroy(expr->ternary.test);
            expression_destroy(expr->ternary.if_true);
            expression_destroy(expr->ternary.if_false);
            break;
        }
    }
    allocator_free(expr->alloc, expr);

}

expression_t* expression_copy(expression_t *expr) {
    if (!expr) {
        return NULL;
    }
    expression_t *res = NULL;
    switch (expr->type) {
        case EXPRESSION_NONE: {
            APE_ASSERT(false);
            break;
        }
        case EXPRESSION_IDENT: {
            ident_t *ident = ident_copy(expr->ident);
            if (!ident) {
                return NULL;
            }
            res = expression_make_ident(expr->alloc, ident);
            if (!res) {
                ident_destroy(ident);
                return NULL;
            }
            break;
        }
        case EXPRESSION_NUMBER_LITERAL: {
            res = expression_make_number_literal(expr->alloc, expr->number_literal);
            break;
        }
        case EXPRESSION_BOOL_LITERAL: {
            res = expression_make_bool_literal(expr->alloc, expr->bool_literal);
            break;
        }
        case EXPRESSION_STRING_LITERAL: {
            char *string_copy = ape_strdup(expr->alloc, expr->string_literal);
            if (!string_copy) {
                return NULL;
            }
            res = expression_make_string_literal(expr->alloc, string_copy);
            if (!res) {
                allocator_free(expr->alloc, string_copy);
                return NULL;
            }
            break;
        }
        case EXPRESSION_NULL_LITERAL: {
            res = expression_make_null_literal(expr->alloc);
            break;
        }
        case EXPRESSION_ARRAY_LITERAL: {
            ptrarray(expression_t) *values_copy = ptrarray_copy_with_items(expr->array, expression_copy, expression_destroy);
            if (!values_copy) {
                return NULL;
            }
            res = expression_make_array_literal(expr->alloc, values_copy);
            if (!res) {
                ptrarray_destroy_with_items(values_copy, expression_destroy);
                return NULL;
            }
            break;
        }
        case EXPRESSION_MAP_LITERAL: {
            ptrarray(expression_t) *keys_copy = ptrarray_copy_with_items(expr->map.keys, expression_copy, expression_destroy);
            ptrarray(expression_t) *values_copy = ptrarray_copy_with_items(expr->map.values, expression_copy, expression_destroy);
            if (!keys_copy || !values_copy) {
                ptrarray_destroy_with_items(keys_copy, expression_destroy);
                ptrarray_destroy_with_items(values_copy, expression_destroy);
                return NULL;
            }
            res = expression_make_map_literal(expr->alloc, keys_copy, values_copy);
            if (!res) {
                ptrarray_destroy_with_items(keys_copy, expression_destroy);
                ptrarray_destroy_with_items(values_copy, expression_destroy);
                return NULL;
            }
            break;
        }
        case EXPRESSION_PREFIX: {
            expression_t *right_copy = expression_copy(expr->prefix.right);
            if (!right_copy) {
                return NULL;
            }
            res = expression_make_prefix(expr->alloc, expr->prefix.op, right_copy);
            if (!res) {
                expression_destroy(right_copy);
                return NULL;
            }
            break;
        }
        case EXPRESSION_INFIX: {
            expression_t *left_copy = expression_copy(expr->infix.left);
            expression_t *right_copy = expression_copy(expr->infix.right);
            if (!left_copy || !right_copy) {
                expression_destroy(left_copy);
                expression_destroy(right_copy);
                return NULL;
            }
            res = expression_make_infix(expr->alloc, expr->infix.op, left_copy, right_copy);
            if (!res) {
                expression_destroy(left_copy);
                expression_destroy(right_copy);
                return NULL;
            }
            break;
        }
        case EXPRESSION_FUNCTION_LITERAL: {
            ptrarray(ident_t) *params_copy = ptrarray_copy_with_items(expr->fn_literal.params, ident_copy, ident_destroy);
            code_block_t *body_copy = code_block_copy(expr->fn_literal.body);
            char *name_copy =  ape_strdup(expr->alloc, expr->fn_literal.name);
            if (!params_copy || !body_copy) {
                ptrarray_destroy_with_items(params_copy, ident_destroy);
                code_block_destroy(body_copy);
                allocator_free(expr->alloc, name_copy);
                return NULL;
            }
            res = expression_make_fn_literal(expr->alloc, params_copy, body_copy);
            if (!res) {
                ptrarray_destroy_with_items(params_copy, ident_destroy);
                code_block_destroy(body_copy);
                allocator_free(expr->alloc, name_copy);
                return NULL;
            }
            res->fn_literal.name = name_copy;
            break;
        }
        case EXPRESSION_CALL: {
            expression_t *function_copy = expression_copy(expr->call_expr.function);
            ptrarray(expression_t) *args_copy = ptrarray_copy_with_items(expr->call_expr.args, expression_copy, expression_destroy);
            if (!function_copy || !args_copy) {
                expression_destroy(function_copy);
                ptrarray_destroy_with_items(expr->call_expr.args, expression_destroy);
                return NULL;
            }
            res = expression_make_call(expr->alloc, function_copy, args_copy);
            if (!res) {
                expression_destroy(function_copy);
                ptrarray_destroy_with_items(expr->call_expr.args, expression_destroy);
                return NULL;
            }
            break;
        }
        case EXPRESSION_INDEX: {
            expression_t *left_copy = expression_copy(expr->index_expr.left);
            expression_t *index_copy = expression_copy(expr->index_expr.index);
            if (!left_copy || !index_copy) {
                expression_destroy(left_copy);
                expression_destroy(index_copy);
                return NULL;
            }
            res = expression_make_index(expr->alloc, left_copy, index_copy);
            if (!res) {
                expression_destroy(left_copy);
                expression_destroy(index_copy);
                return NULL;
            }
            break;
        }
        case EXPRESSION_ASSIGN: {
            expression_t *dest_copy = expression_copy(expr->assign.dest);
            expression_t *source_copy = expression_copy(expr->assign.source);
            if (!dest_copy || !source_copy) {
                expression_destroy(dest_copy);
                expression_destroy(source_copy);
                return NULL;
            }
            res = expression_make_assign(expr->alloc, dest_copy, source_copy, expr->assign.is_postfix);
            if (!res) {
                expression_destroy(dest_copy);
                expression_destroy(source_copy);
                return NULL;
            }
            break;
        }
        case EXPRESSION_LOGICAL: {
            expression_t *left_copy = expression_copy(expr->logical.left);
            expression_t *right_copy = expression_copy(expr->logical.right);
            if (!left_copy || !right_copy) {
                expression_destroy(left_copy);
                expression_destroy(right_copy);
                return NULL;
            }
            res = expression_make_logical(expr->alloc, expr->logical.op, left_copy, right_copy);
            if (!res) {
                expression_destroy(left_copy);
                expression_destroy(right_copy);
                return NULL;
            }
            break;
        }
        case EXPRESSION_TERNARY: {
            expression_t *test_copy = expression_copy(expr->ternary.test);
            expression_t *if_true_copy = expression_copy(expr->ternary.if_true);
            expression_t *if_false_copy = expression_copy(expr->ternary.if_false);
            if (!test_copy || !if_true_copy || !if_false_copy) {
                expression_destroy(test_copy);
                expression_destroy(if_true_copy);
                expression_destroy(if_false_copy);
                return NULL;
            }
            res = expression_make_ternary(expr->alloc, test_copy, if_true_copy, if_false_copy);
            if (!res) {
                expression_destroy(test_copy);
                expression_destroy(if_true_copy);
                expression_destroy(if_false_copy);
                return NULL;
            }
            break;
        }
    }
    if (!res) {
        return NULL;
    }
    res->pos = expr->pos;
    return res;
}

statement_t* statement_make_define(allocator_t *alloc, ident_t *name, int type,
                                   expression_t *value, bool assignable) {
    statement_t *res = statement_make(alloc, STATEMENT_DEFINE);
    if (!res) {
        return NULL;
    }
    res->define.name = name;
    res->define.type = type;
    res->define.value = value;
    res->define.assignable = assignable;
    return res;
}

statement_t* statement_make_if(allocator_t *alloc, ptrarray(if_case_t) *cases, code_block_t *alternative) {
    statement_t *res = statement_make(alloc, STATEMENT_IF);
    if (!res) {
        return NULL;
    }
    res->if_statement.cases = cases;
    res->if_statement.alternative = alternative;
    return res;
}

statement_t* statement_make_return(allocator_t *alloc, expression_t *value) {
    statement_t *res = statement_make(alloc, STATEMENT_RETURN_VALUE);
    if (!res) {
        return NULL;
    }
    res->return_value = value;
    return res;
}

statement_t* statement_make_expression(allocator_t *alloc, expression_t *value) {
    statement_t *res = statement_make(alloc, STATEMENT_EXPRESSION);
    if (!res) {
        return NULL;
    }
    res->expression = value;
    return res;
}

statement_t* statement_make_while_loop(allocator_t *alloc, expression_t *test, code_block_t *body) {
    statement_t *res = statement_make(alloc, STATEMENT_WHILE_LOOP);
    if (!res) {
        return NULL;
    }
    res->while_loop.test = test;
    res->while_loop.body = body;
    return res;
}

statement_t* statement_make_break(allocator_t *alloc) {
    statement_t *res = statement_make(alloc, STATEMENT_BREAK);
    if (!res) {
        return NULL;
    }
    return res;
}

statement_t* statement_make_foreach(allocator_t *alloc, ident_t *iterator, expression_t *source, code_block_t *body) {
    statement_t *res = statement_make(alloc, STATEMENT_FOREACH);
    if (!res) {
        return NULL;
    }
    res->foreach.iterator = iterator;
    res->foreach.source = source;
    res->foreach.body = body;
    return res;
}

statement_t* statement_make_for_loop(allocator_t *alloc, statement_t *init, expression_t *test, expression_t *update, code_block_t *body) {
    statement_t *res = statement_make(alloc, STATEMENT_FOR_LOOP);
    if (!res) {
        return NULL;
    }
    res->for_loop.init = init;
    res->for_loop.test = test;
    res->for_loop.update = update;
    res->for_loop.body = body;
    return res;
}

statement_t* statement_make_continue(allocator_t *alloc) {
    statement_t *res = statement_make(alloc, STATEMENT_CONTINUE);
    if (!res) {
        return NULL;
    }
    return res;
}

statement_t* statement_make_block(allocator_t *alloc, code_block_t *block) {
    statement_t *res = statement_make(alloc, STATEMENT_BLOCK);
    if (!res) {
        return NULL;
    }
    res->block = block;
    return res;
}

statement_t* statement_make_import(allocator_t *alloc, char *path) {
    statement_t *res = statement_make(alloc, STATEMENT_IMPORT);
    if (!res) {
        return NULL;
    }
    res->import.path = path;
    return res;
}

statement_t* statement_make_recover(allocator_t *alloc, ident_t *error_ident, code_block_t *body) {
    statement_t *res = statement_make(alloc, STATEMENT_RECOVER);
    if (!res) {
        return NULL;
    }
    res->recover.error_ident = error_ident;
    res->recover.body = body;
    return res;
}

void statement_destroy(statement_t *stmt) {
    if (!stmt) {
        return;
    }
    switch (stmt->type) {
        case STATEMENT_NONE: {
            APE_ASSERT(false);
            break;
        }
        case STATEMENT_DEFINE: {
            ident_destroy(stmt->define.name);
            expression_destroy(stmt->define.value);
            break;
        }
        case STATEMENT_IF: {
            ptrarray_destroy_with_items(stmt->if_statement.cases, if_case_destroy);
            code_block_destroy(stmt->if_statement.alternative);
            break;
        }
        case STATEMENT_RETURN_VALUE: {
            expression_destroy(stmt->return_value);
            break;
        }
        case STATEMENT_EXPRESSION: {
            expression_destroy(stmt->expression);
            break;
        }
        case STATEMENT_WHILE_LOOP: {
            expression_destroy(stmt->while_loop.test);
            code_block_destroy(stmt->while_loop.body);
            break;
        }
        case STATEMENT_BREAK: {
            break;
        }
        case STATEMENT_CONTINUE: {
            break;
        }
        case STATEMENT_FOREACH: {
            ident_destroy(stmt->foreach.iterator);
            expression_destroy(stmt->foreach.source);
            code_block_destroy(stmt->foreach.body);
            break;
        }
        case STATEMENT_FOR_LOOP: {
            statement_destroy(stmt->for_loop.init);
            expression_destroy(stmt->for_loop.test);
            expression_destroy(stmt->for_loop.update);
            code_block_destroy(stmt->for_loop.body);
            break;
        }
        case STATEMENT_BLOCK: {
            code_block_destroy(stmt->block);
            break;
        }
        case STATEMENT_IMPORT: {
            allocator_free(stmt->alloc, stmt->import.path);
            break;
        }
        case STATEMENT_RECOVER: {
            code_block_destroy(stmt->recover.body);
            ident_destroy(stmt->recover.error_ident);
            break;
        }
    }
    allocator_free(stmt->alloc, stmt);
}

statement_t* statement_copy(const statement_t *stmt) {
    if (!stmt) {
        return NULL;
    }
    statement_t *res = NULL;
    switch (stmt->type) {
        case STATEMENT_NONE: {
            APE_ASSERT(false);
            break;
        }
        case STATEMENT_DEFINE: {
            expression_t *value_copy = expression_copy(stmt->define.value);
            if (!value_copy) {
                return NULL;
            }
            res = statement_make_define(stmt->alloc, ident_copy(stmt->define.name), value_copy, stmt->define.assignable);
            if (!res) {
                expression_destroy(value_copy);
                return NULL;
            }
            break;
        }
        case STATEMENT_IF: {
            ptrarray(if_case_t) *cases_copy = ptrarray_copy_with_items(stmt->if_statement.cases, if_case_copy, if_case_destroy);
            code_block_t *alternative_copy = code_block_copy(stmt->if_statement.alternative);
            if (!cases_copy || !alternative_copy) {
                ptrarray_destroy_with_items(cases_copy, if_case_destroy);
                code_block_destroy(alternative_copy);
                return NULL;
            }
            res = statement_make_if(stmt->alloc, cases_copy, alternative_copy);
            if (res) {
                ptrarray_destroy_with_items(cases_copy, if_case_destroy);
                code_block_destroy(alternative_copy);
                return NULL;
            }
            break;
        }
        case STATEMENT_RETURN_VALUE: {
            expression_t *value_copy = expression_copy(stmt->return_value);
            if (!value_copy) {
                return NULL;
            }
            res = statement_make_return(stmt->alloc, value_copy);
            if (!res) {
                expression_destroy(value_copy);
                return NULL;
            }
            break;
        }
        case STATEMENT_EXPRESSION: {
            expression_t *value_copy = expression_copy(stmt->expression);
            if (!value_copy) {
                return NULL;
            }
            res = statement_make_expression(stmt->alloc, value_copy);
            if (!res) {
                expression_destroy(value_copy);
                return NULL;
            }
            break;
        }
        case STATEMENT_WHILE_LOOP: {
            expression_t *test_copy = expression_copy(stmt->while_loop.test);
            code_block_t *body_copy = code_block_copy(stmt->while_loop.body);
            if (!test_copy || !body_copy) {
                expression_destroy(test_copy);
                code_block_destroy(body_copy);
                return NULL;
            }
            res = statement_make_while_loop(stmt->alloc, test_copy, body_copy);
            if (!res) {
                expression_destroy(test_copy);
                code_block_destroy(body_copy);
                return NULL;
            }
            break;
        }
        case STATEMENT_BREAK: {
            res = statement_make_break(stmt->alloc);
            break;
        }
        case STATEMENT_CONTINUE: {
            res = statement_make_continue(stmt->alloc);
            break;
        }
        case STATEMENT_FOREACH: {
            expression_t *source_copy = expression_copy(stmt->foreach.source);
            code_block_t *body_copy = code_block_copy(stmt->foreach.body);
            if (!source_copy || !body_copy) {
                expression_destroy(source_copy);
                code_block_destroy(body_copy);
                return NULL;
            }
            res = statement_make_foreach(stmt->alloc, ident_copy(stmt->foreach.iterator), source_copy, body_copy);
            if (!res) {
                expression_destroy(source_copy);
                code_block_destroy(body_copy);
                return NULL;
            }
            break;
        }
        case STATEMENT_FOR_LOOP: {
            statement_t *init_copy = statement_copy(stmt->for_loop.init);
            expression_t *test_copy = expression_copy(stmt->for_loop.test);
            expression_t *update_copy = expression_copy(stmt->for_loop.update);
            code_block_t *body_copy = code_block_copy(stmt->for_loop.body);
            if (!init_copy || !test_copy || !update_copy || !body_copy) {
                statement_destroy(init_copy);
                expression_destroy(test_copy);
                expression_destroy(update_copy);
                code_block_destroy(body_copy);
                return NULL;
            }
            res = statement_make_for_loop(stmt->alloc, init_copy, test_copy, update_copy, body_copy);
            if (!res) {
                statement_destroy(init_copy);
                expression_destroy(test_copy);
                expression_destroy(update_copy);
                code_block_destroy(body_copy);
                return NULL;
            }
            break;
        }
        case STATEMENT_BLOCK: {
            code_block_t *block_copy = code_block_copy(stmt->block);
            if (!block_copy) {
                return NULL;
            }
            res = statement_make_block(stmt->alloc, block_copy);
            if (!res) {
                code_block_destroy(block_copy);
                return NULL;
            }
            break;
        }
        case STATEMENT_IMPORT: {
            char *path_copy = ape_strdup(stmt->alloc, stmt->import.path);
            if (!path_copy) {
                return NULL;
            }
            res = statement_make_import(stmt->alloc, path_copy);
            if (!res) {
                allocator_free(stmt->alloc, path_copy);
                return NULL;
            }
            break;
        }
        case STATEMENT_RECOVER: {
            code_block_t *body_copy = code_block_copy(stmt->recover.body);
            ident_t *error_ident_copy = ident_copy(stmt->recover.error_ident);
            if (!body_copy || !error_ident_copy) {
                code_block_destroy(body_copy);
                ident_destroy(error_ident_copy);
                return NULL;
            }
            res = statement_make_recover(stmt->alloc, error_ident_copy, body_copy);
            if (!res) {
                code_block_destroy(body_copy);
                ident_destroy(error_ident_copy);
                return NULL;
            }
            break;
        }
    }
    if (!res) {
        return NULL;
    }
    res->pos = stmt->pos;
    return res;
}

code_block_t* code_block_make(allocator_t *alloc, ptrarray(statement_t) *statements) {
    code_block_t *block = allocator_malloc(alloc, sizeof(code_block_t));
    if (!block) {
        return NULL;
    }
    block->alloc = alloc;
    block->statements = statements;
    return block;
}

void code_block_destroy(code_block_t *block) {
    if (!block) {
        return;
    }
    ptrarray_destroy_with_items(block->statements, statement_destroy);
    allocator_free(block->alloc, block);
}

code_block_t* code_block_copy(code_block_t *block) {
    if (!block) {
        return NULL;
    }
    ptrarray(statement_t) *statements_copy = ptrarray_copy_with_items(block->statements, statement_copy, statement_destroy);
    if (!statements_copy) {
        return NULL;
    }
    code_block_t* res = code_block_make(block->alloc, statements_copy);
    if (!res) {
        ptrarray_destroy_with_items(statements_copy, statement_destroy);
        return NULL;
    }
    return res;
}

char* statements_to_string(allocator_t *alloc, ptrarray(statement_t) *statements) {
    strbuf_t *buf = strbuf_make(alloc);
    if (!buf) {
        return NULL;
    }
    int count =  ptrarray_count(statements);
    for (int i = 0; i < count; i++) {
        const statement_t *stmt = ptrarray_get(statements, i);
        statement_to_string(stmt, buf);
        if (i < (count - 1)) {
            strbuf_append(buf, "\n");
        }
    }
    return strbuf_get_string_and_destroy(buf);
}

void statement_to_string(const statement_t *stmt, strbuf_t *buf) {
    switch (stmt->type) {
        case STATEMENT_DEFINE: {
            const define_statement_t *def_stmt = &stmt->define;
            if (stmt->define.assignable) {
                strbuf_append(buf, "var ");
            } else {
                strbuf_append(buf, "const ");
            }
            strbuf_append(buf, def_stmt->name->value);
            strbuf_append(buf, " = ");

            if (def_stmt->value) {
                expression_to_string(def_stmt->value, buf);
            }

            break;
        }
        case STATEMENT_IF: {
            if_case_t *if_case = ptrarray_get(stmt->if_statement.cases, 0);
            strbuf_append(buf, "if (");
            expression_to_string(if_case->test, buf);
            strbuf_append(buf, ") ");
            code_block_to_string(if_case->consequence, buf);
            for (int i = 1; i < ptrarray_count(stmt->if_statement.cases); i++) {
                if_case_t *elif_case = ptrarray_get(stmt->if_statement.cases, i);
                strbuf_append(buf, " elif (");
                expression_to_string(elif_case->test, buf);
                strbuf_append(buf, ") ");
                code_block_to_string(elif_case->consequence, buf);
            }
            if (stmt->if_statement.alternative) {
                strbuf_append(buf, " else ");
                code_block_to_string(stmt->if_statement.alternative, buf);
            }
            break;
        }
        case STATEMENT_RETURN_VALUE: {
            strbuf_append(buf, "return ");
            if (stmt->return_value) {
                expression_to_string(stmt->return_value, buf);
            }
            break;
        }
        case STATEMENT_EXPRESSION: {
            if (stmt->expression) {
                expression_to_string(stmt->expression, buf);
            }
            break;
        }
        case STATEMENT_WHILE_LOOP: {
            strbuf_append(buf, "while (");
            expression_to_string(stmt->while_loop.test, buf);
            strbuf_append(buf, ")");
            code_block_to_string(stmt->while_loop.body, buf);
            break;
        }
        case STATEMENT_FOR_LOOP: {
            strbuf_append(buf, "for (");
            if (stmt->for_loop.init) {
                statement_to_string(stmt->for_loop.init, buf);
                strbuf_append(buf, " ");
            } else {
                strbuf_append(buf, ";");
            }
            if (stmt->for_loop.test) {
                expression_to_string(stmt->for_loop.test, buf);
                strbuf_append(buf, "; ");
            } else {
                strbuf_append(buf, ";");
            }
            if (stmt->for_loop.update) {
                expression_to_string(stmt->for_loop.test, buf);
            }
            strbuf_append(buf, ")");
            code_block_to_string(stmt->for_loop.body, buf);
            break;
        }
        case STATEMENT_FOREACH: {
            strbuf_append(buf, "for (");
            strbuf_appendf(buf, "%s", stmt->foreach.iterator->value);
            strbuf_append(buf, " in ");
            expression_to_string(stmt->foreach.source, buf);
            strbuf_append(buf, ")");
            code_block_to_string(stmt->foreach.body, buf);
            break;
        }
        case STATEMENT_BLOCK: {
            code_block_to_string(stmt->block, buf);
            break;
        }
        case STATEMENT_BREAK: {
            strbuf_append(buf, "break");
            break;
        }
        case STATEMENT_CONTINUE: {
            strbuf_append(buf, "continue");
            break;
        }
        case STATEMENT_IMPORT: {
            strbuf_appendf(buf, "import \"%s\"", stmt->import.path);
            break;
        }
        case STATEMENT_NONE: {
            strbuf_append(buf, "STATEMENT_NONE");
            break;
        }
        case STATEMENT_RECOVER: {
            strbuf_appendf(buf, "recover (%s)", stmt->recover.error_ident->value);
            code_block_to_string(stmt->recover.body, buf);
            break;
        }
    }
}

void expression_to_string(expression_t *expr, strbuf_t *buf) {
    switch (expr->type) {
        case EXPRESSION_IDENT: {
            strbuf_append(buf, expr->ident->value);
            break;
        }
        case EXPRESSION_NUMBER_LITERAL: {
            strbuf_appendf(buf, "%1.17g", expr->number_literal);
            break;
        }
        case EXPRESSION_BOOL_LITERAL: {
            strbuf_appendf(buf, "%s", expr->bool_literal ? "true" : "false");
            break;
        }
        case EXPRESSION_STRING_LITERAL: {
            strbuf_appendf(buf, "\"%s\"", expr->string_literal);
            break;
        }
        case EXPRESSION_NULL_LITERAL: {
            strbuf_append(buf, "null");
            break;
        }
        case EXPRESSION_ARRAY_LITERAL: {
            strbuf_append(buf, "[");
            for (int i = 0; i < ptrarray_count(expr->array); i++) {
                expression_t *arr_expr = ptrarray_get(expr->array, i);
                expression_to_string(arr_expr, buf);
                if (i < (ptrarray_count(expr->array) - 1)) {
                    strbuf_append(buf, ", ");
                }
            }
            strbuf_append(buf, "]");
            break;
        }
        case EXPRESSION_MAP_LITERAL: {
            map_literal_t *map = &expr->map;

            strbuf_append(buf, "{");
            for (int i = 0; i < ptrarray_count(map->keys); i++) {
                expression_t *key_expr = ptrarray_get(map->keys, i);
                expression_t *val_expr = ptrarray_get(map->values, i);

                expression_to_string(key_expr, buf);
                strbuf_append(buf, " : ");
                expression_to_string(val_expr, buf);

                if (i < (ptrarray_count(map->keys) - 1)) {
                    strbuf_append(buf, ", ");
                }
            }
            strbuf_append(buf, "}");
            break;
        }
        case EXPRESSION_PREFIX: {
            strbuf_append(buf, "(");
            strbuf_append(buf, operator_to_string(expr->infix.op));
            expression_to_string(expr->prefix.right, buf);
            strbuf_append(buf, ")");
            break;
        }
        case EXPRESSION_INFIX: {
            strbuf_append(buf, "(");
            expression_to_string(expr->infix.left, buf);
            strbuf_append(buf, " ");
            strbuf_append(buf, operator_to_string(expr->infix.op));
            strbuf_append(buf, " ");
            expression_to_string(expr->infix.right, buf);
            strbuf_append(buf, ")");
            break;
        }
        case EXPRESSION_FUNCTION_LITERAL: {
            fn_literal_t *fn = &expr->fn_literal;
            
            strbuf_append(buf, "fn");

            strbuf_append(buf, "(");
            for (int i = 0; i < ptrarray_count(fn->params); i++) {
                ident_t *param = ptrarray_get(fn->params, i);
                strbuf_append(buf, param->value);
                if (i < (ptrarray_count(fn->params) - 1)) {
                    strbuf_append(buf, ", ");
                }
            }
            strbuf_append(buf, ") ");

            code_block_to_string(fn->body, buf);

            break;
        }
        case EXPRESSION_CALL: {
            call_expression_t *call_expr = &expr->call_expr;

            expression_to_string(call_expr->function, buf);

            strbuf_append(buf, "(");
            for (int i = 0; i < ptrarray_count(call_expr->args); i++) {
                expression_t *arg = ptrarray_get(call_expr->args, i);
                expression_to_string(arg, buf);
                if (i < (ptrarray_count(call_expr->args) - 1)) {
                    strbuf_append(buf, ", ");
                }
            }
            strbuf_append(buf, ")");

            break;
        }
        case EXPRESSION_INDEX: {
            strbuf_append(buf, "(");
            expression_to_string(expr->index_expr.left, buf);
            strbuf_append(buf, "[");
            expression_to_string(expr->index_expr.index, buf);
            strbuf_append(buf, "])");
            break;
        }
        case EXPRESSION_ASSIGN: {
            expression_to_string(expr->assign.dest, buf);
            strbuf_append(buf, " = ");
            expression_to_string(expr->assign.source, buf);
            break;
        }
        case EXPRESSION_LOGICAL: {
            expression_to_string(expr->logical.left, buf);
            strbuf_append(buf, " ");
            strbuf_append(buf, operator_to_string(expr->infix.op));
            strbuf_append(buf, " ");
            expression_to_string(expr->logical.right, buf);
            break;
        }
        case EXPRESSION_TERNARY: {
            expression_to_string(expr->ternary.test, buf);
            strbuf_append(buf, " ? ");
            expression_to_string(expr->ternary.if_true, buf);
            strbuf_append(buf, " : ");
            expression_to_string(expr->ternary.if_false, buf);
            break;
        }
        case EXPRESSION_NONE: {
            strbuf_append(buf, "EXPRESSION_NONE");
            break;
        }
    }
}

void code_block_to_string(const code_block_t *stmt, strbuf_t *buf) {
    strbuf_append(buf, "{ ");
    for (int i = 0; i < ptrarray_count(stmt->statements); i++) {
        statement_t *istmt = ptrarray_get(stmt->statements, i);
        statement_to_string(istmt, buf);
        strbuf_append(buf, "\n");
    }
    strbuf_append(buf, " }");
}

const char* operator_to_string(operator_t op) {
    switch (op) {
        case OPERATOR_NONE:        return "OPERATOR_NONE";
        case OPERATOR_ASSIGN:      return "=";
        case OPERATOR_PLUS:        return "+";
        case OPERATOR_MINUS:       return "-";
        case OPERATOR_BANG:        return "!";
        case OPERATOR_ASTERISK:    return "*";
        case OPERATOR_SLASH:       return "/";
        case OPERATOR_LT:          return "<";
        case OPERATOR_GT:          return ">";
        case OPERATOR_EQ:          return "==";
        case OPERATOR_NOT_EQ:      return "!=";
        case OPERATOR_MODULUS:     return "%";
        case OPERATOR_LOGICAL_AND: return "&&";
        case OPERATOR_LOGICAL_OR:  return "||";
        case OPERATOR_BIT_AND:     return "&";
        case OPERATOR_BIT_OR:      return "|";
        case OPERATOR_BIT_XOR:     return "^";
        case OPERATOR_LSHIFT:      return "<<";
        case OPERATOR_RSHIFT:      return ">>";
        default:                   return "OPERATOR_UNKNOWN";
    }
}

const char *expression_type_to_string(expression_type_t type) {
    switch (type) {
        case EXPRESSION_NONE:             return "NONE";
        case EXPRESSION_IDENT:            return "IDENT";
        case EXPRESSION_NUMBER_LITERAL:   return "INT_LITERAL";
        case EXPRESSION_BOOL_LITERAL:     return "BOOL_LITERAL";
        case EXPRESSION_STRING_LITERAL:   return "STRING_LITERAL";
        case EXPRESSION_ARRAY_LITERAL:    return "ARRAY_LITERAL";
        case EXPRESSION_MAP_LITERAL:      return "MAP_LITERAL";
        case EXPRESSION_PREFIX:           return "PREFIX";
        case EXPRESSION_INFIX:            return "INFIX";
        case EXPRESSION_FUNCTION_LITERAL: return "FN_LITERAL";
        case EXPRESSION_CALL:             return "CALL";
        case EXPRESSION_INDEX:            return "INDEX";
        case EXPRESSION_ASSIGN:           return "ASSIGN";
        case EXPRESSION_LOGICAL:          return "LOGICAL";
        case EXPRESSION_TERNARY:          return "TERNARY";
        default:                          return "UNKNOWN";
    }
}

ident_t* ident_make(allocator_t *alloc, token_t tok) {
    ident_t *res = allocator_malloc(alloc, sizeof(ident_t));
    if (!res) {
        return NULL;
    }
    res->alloc = alloc;
    res->value = token_duplicate_literal(alloc, &tok);
    if (!res->value) {
        allocator_free(alloc, res);
        return NULL;
    }
    res->pos = tok.pos;
    return res;
}

ident_t* ident_copy(ident_t *ident) {
    ident_t *res = allocator_malloc(ident->alloc, sizeof(ident_t));
    if (!res) {
        return NULL;
    }
    res->alloc = ident->alloc;
    res->value = ape_strdup(ident->alloc, ident->value);
    if (!res->value) {
        allocator_free(ident->alloc, res);
        return NULL;
    }
    res->pos = ident->pos;
    return res;
}

void ident_destroy(ident_t *ident) {
    if (!ident) {
        return;
    }
    allocator_free(ident->alloc, ident->value);
    ident->value = NULL;
    ident->pos = src_pos_invalid;
    allocator_free(ident->alloc, ident);
}

if_case_t *if_case_make(allocator_t *alloc, expression_t *test, code_block_t *consequence) {
    if_case_t *res = allocator_malloc(alloc, sizeof(if_case_t));
    if (!res) {
        return NULL;
    }
    res->alloc = alloc;
    res->test = test;
    res->consequence = consequence;
    return res;
}

void if_case_destroy(if_case_t *cond) {
    if (!cond) {
        return;
    }
    expression_destroy(cond->test);
    code_block_destroy(cond->consequence);
    allocator_free(cond->alloc, cond);
}

if_case_t* if_case_copy(if_case_t *if_case) {
    if (!if_case) {
        return NULL;
    }
    expression_t *test_copy = NULL;
    code_block_t *consequence_copy = NULL;
    if_case_t *if_case_copy = NULL;

    test_copy = expression_copy(if_case->test);
    if (!test_copy) {
        goto err;
    }
    consequence_copy = code_block_copy(if_case->consequence);
    if (!test_copy || !consequence_copy) {
        goto err;
    }
    if_case_copy = if_case_make(if_case->alloc, test_copy, consequence_copy);
    if (!if_case_copy) {
        goto err;
    }
    return if_case_copy;
err:
    expression_destroy(test_copy);
    code_block_destroy(consequence_copy);
    if_case_destroy(if_case_copy);
    return NULL;
}

// INTERNAL
static expression_t *expression_make(allocator_t *alloc, expression_type_t type) {
    expression_t *res = allocator_malloc(alloc, sizeof(expression_t));
    if (!res) {
        return NULL;
    }
    res->alloc = alloc;
    res->type = type;
    res->pos = src_pos_invalid;
    return res;
}

static statement_t* statement_make(allocator_t *alloc, statement_type_t type) {
    statement_t *res = allocator_malloc(alloc, sizeof(statement_t));
    if (!res) {
        return NULL;
    }
    res->alloc = alloc;
    res->type = type;
    res->pos = src_pos_invalid;
    return res;
}
