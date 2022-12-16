#ifndef lexer_h
#define lexer_h

#include <stdbool.h>
#include <stddef.h>

#ifndef APE_AMALGAMATED
#include "common.h"
#include "token.h"
#include "collections.h"
#include "compiled_file.h"
#endif

typedef struct errors errors_t;

typedef struct lexer {
    allocator_t *alloc;
    errors_t *errors;
    const char *input;
    int input_len;
    int position;
    int next_position;
    char ch;
    int line;
    int column;
    compiled_file_t *file;
    bool failed;
    bool continue_template_string;
    struct {
        int position;
        int next_position;
        char ch;
        int line;
        int column;
    } prev_token_state;
    token_t prev_token;
    token_t cur_token;
    token_t peek_token;
} lexer_t;

APE_INTERNAL bool lexer_init(lexer_t *lex, allocator_t *alloc, errors_t *errs, const char *input, compiled_file_t *file); // no need to deinit

APE_INTERNAL bool lexer_failed(lexer_t *lex);
APE_INTERNAL void lexer_continue_template_string(lexer_t *lex);
APE_INTERNAL bool lexer_cur_token_is(lexer_t *lex, token_type_t type);
APE_INTERNAL bool lexer_peek_token_is(lexer_t *lex, token_type_t type);
APE_INTERNAL bool lexer_next_token(lexer_t *lex);
APE_INTERNAL bool lexer_previous_token(lexer_t *lex);
APE_INTERNAL token_t lexer_next_token_internal(lexer_t *lex); // exposed here for tests
APE_INTERNAL bool lexer_expect_current(lexer_t *lex, token_type_t type);

#endif /* lexer_h */
