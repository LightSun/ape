#ifndef compiler_h
#define compiler_h

#ifndef APE_AMALGAMATED
#include "collections.h"
#include "common.h"
#include "errors.h"
#include "parser.h"
#include "code.h"
#include "token.h"
#include "compilation_scope.h"
#include "compiled_file.h"
#endif

typedef struct ape_config ape_config_t;
typedef struct gcmem gcmem_t;
typedef struct symbol_table symbol_table_t;

typedef struct compiler compiler_t;
typedef struct compilation_result compilation_result_t;
typedef struct compiled_file compiled_file_t;

APE_INTERNAL compiler_t *compiler_make(allocator_t *alloc,
                                       const ape_config_t *config,
                                       gcmem_t *mem, errors_t *errors,
                                       ptrarray(compiled_file_t) *files,
                                       global_store_t *global_store);
APE_INTERNAL void compiler_destroy(compiler_t *comp);
APE_INTERNAL compilation_result_t* compiler_compile(compiler_t *comp, const char *code);
APE_INTERNAL compilation_result_t* compiler_compile_file(compiler_t *comp, const char *path);
APE_INTERNAL symbol_table_t* compiler_get_symbol_table(compiler_t *comp);
APE_INTERNAL void compiler_set_symbol_table(compiler_t *comp, symbol_table_t *table);
APE_INTERNAL array(object_t)* compiler_get_constants(const compiler_t *comp);

#endif /* compiler_h */
