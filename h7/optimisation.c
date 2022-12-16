#ifndef APE_AMALGAMATED
#include "optimisation.h"
#endif

static expression_t* optimise_infix_expression(expression_t* expr);
static expression_t* optimise_prefix_expression(expression_t* expr);

expression_t* optimise_expression(expression_t* expr) {
    switch (expr->type) {
        case EXPRESSION_INFIX: return optimise_infix_expression(expr);
        case EXPRESSION_PREFIX: return optimise_prefix_expression(expr);
        default: return NULL;
    }
}

// INTERNAL
static expression_t* optimise_infix_expression(expression_t* expr) {
    expression_t *left = expr->infix.left;
    expression_t *left_optimised = optimise_expression(left);
    if (left_optimised) {
        left = left_optimised;
    }

    expression_t *right = expr->infix.right;
    expression_t *right_optimised = optimise_expression(right);
    if (right_optimised) {
        right = right_optimised;
    }

    expression_t *res = NULL;

    bool left_is_numeric = left->type == EXPRESSION_NUMBER_LITERAL || left->type == EXPRESSION_BOOL_LITERAL;
    bool right_is_numeric = right->type == EXPRESSION_NUMBER_LITERAL || right->type == EXPRESSION_BOOL_LITERAL;

    bool left_is_string = left->type == EXPRESSION_STRING_LITERAL;
    bool right_is_string = right->type == EXPRESSION_STRING_LITERAL;

    allocator_t *alloc = expr->alloc;
    if (left_is_numeric && right_is_numeric) {
        double left_val = left->type == EXPRESSION_NUMBER_LITERAL ? left->number_literal : left->bool_literal;
        double right_val = right->type == EXPRESSION_NUMBER_LITERAL ? right->number_literal : right->bool_literal;
        int64_t left_val_int = (int64_t)left_val;
        int64_t right_val_int = (int64_t)right_val;
        switch (expr->infix.op) {
            case OPERATOR_PLUS:     { res = expression_make_number_literal(alloc, left_val + right_val); break; }
            case OPERATOR_MINUS:    { res = expression_make_number_literal(alloc, left_val - right_val); break; }
            case OPERATOR_ASTERISK: { res = expression_make_number_literal(alloc, left_val * right_val); break; }
            case OPERATOR_SLASH:    { res = expression_make_number_literal(alloc, left_val / right_val); break; }
            case OPERATOR_LT:       { res = expression_make_bool_literal(alloc, left_val < right_val); break; }
            case OPERATOR_LTE:      { res = expression_make_bool_literal(alloc, left_val <= right_val); break; }
            case OPERATOR_GT:       { res = expression_make_bool_literal(alloc, left_val > right_val); break; }
            case OPERATOR_GTE:      { res = expression_make_bool_literal(alloc, left_val >= right_val); break; }
            case OPERATOR_EQ:       { res = expression_make_bool_literal(alloc, APE_DBLEQ(left_val, right_val)); break; }
            case OPERATOR_NOT_EQ:   { res = expression_make_bool_literal(alloc, !APE_DBLEQ(left_val, right_val)); break; }
            case OPERATOR_MODULUS:  { res = expression_make_number_literal(alloc, fmod(left_val, right_val)); break; }
            case OPERATOR_BIT_AND:  { res = expression_make_number_literal(alloc, (double)(left_val_int & right_val_int)); break; }
            case OPERATOR_BIT_OR:   { res = expression_make_number_literal(alloc, (double)(left_val_int | right_val_int)); break; }
            case OPERATOR_BIT_XOR:  { res = expression_make_number_literal(alloc, (double)(left_val_int ^ right_val_int)); break; }
            case OPERATOR_LSHIFT:   { res = expression_make_number_literal(alloc, (double)(left_val_int << right_val_int)); break; }
            case OPERATOR_RSHIFT:   { res = expression_make_number_literal(alloc, (double)(left_val_int >> right_val_int)); break; }
            default: {
                break;
            }
        }
    } else if (expr->infix.op == OPERATOR_PLUS && left_is_string && right_is_string) {
        const char* left_val = left->string_literal;
        const char* right_val = right->string_literal;
        char *res_str = ape_stringf(alloc, "%s%s", left_val, right_val);
        if (res_str) {
            res = expression_make_string_literal(alloc, res_str);
            if (!res) {
                allocator_free(alloc, res_str);
            }
        }
    }

    expression_destroy(left_optimised);
    expression_destroy(right_optimised);

    if (res) {
        res->pos = expr->pos;
    }

    return res;
}

expression_t* optimise_prefix_expression(expression_t* expr) {
    expression_t *right = expr->prefix.right;
    expression_t *right_optimised = optimise_expression(right);
    if (right_optimised) {
        right = right_optimised;
    }
    expression_t *res = NULL;
    if (expr->prefix.op == OPERATOR_MINUS && right->type == EXPRESSION_NUMBER_LITERAL) {
        res = expression_make_number_literal(expr->alloc, -right->number_literal);
    } else if (expr->prefix.op == OPERATOR_BANG && right->type == EXPRESSION_BOOL_LITERAL) {
        res = expression_make_bool_literal(expr->alloc, !right->bool_literal);
    }
    expression_destroy(right_optimised);
    if (res) {
        res->pos = expr->pos;
    }
    return res;
}
