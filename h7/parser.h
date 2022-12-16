#ifndef parser_h
#define parser_h

#ifndef APE_AMALGAMATED
#include "common.h"
#include "lexer.h"
#include "token.h"
#include "ast.h"
#include "collections.h"
#endif

typedef struct parser parser_t;
typedef struct errors errors_t;

typedef expression_t* (*right_assoc_parse_fn)(parser_t *p);
typedef expression_t* (*left_assoc_parse_fn)(parser_t *p, expression_t *expr);

typedef struct parser {
    allocator_t *alloc;
    const ape_config_t *config;
    lexer_t lexer;
    errors_t *errors;
    
    right_assoc_parse_fn right_assoc_parse_fns[TOKEN_TYPE_MAX];
    left_assoc_parse_fn left_assoc_parse_fns[TOKEN_TYPE_MAX];

    int depth;
} parser_t;

APE_INTERNAL parser_t* parser_make(allocator_t *alloc, const ape_config_t *config, errors_t *errors);
APE_INTERNAL void parser_destroy(parser_t *parser);

APE_INTERNAL ptrarray(statement_t)* parser_parse_all(parser_t *parser,  const char *input, compiled_file_t *file);

#endif /* parser_h */
