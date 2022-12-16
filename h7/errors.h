#ifndef errors_h
#define errors_h

#ifndef APE_AMALGAMATED
#include "common.h"
#include "token.h"
#endif

#define ERRORS_MAX_COUNT 16
#define ERROR_MESSAGE_MAX_LENGTH 255

typedef struct traceback traceback_t;

typedef enum error_type {
    ERROR_NONE = 0,
    ERROR_PARSING,
    ERROR_COMPILATION,
    ERROR_RUNTIME,
    ERROR_TIMEOUT,
    ERROR_ALLOCATION,
    ERROR_USER,
} error_type_t;

typedef struct error {
    error_type_t type;
    char message[ERROR_MESSAGE_MAX_LENGTH];
    src_pos_t pos;
    traceback_t *traceback;
} error_t;

typedef struct errors {
    error_t errors[ERRORS_MAX_COUNT];
    int count;
} errors_t;

APE_INTERNAL void errors_init(errors_t *errors);
APE_INTERNAL void errors_deinit(errors_t *errors);

APE_INTERNAL void errors_add_error(errors_t *errors, error_type_t type, src_pos_t pos, const char *message);
APE_INTERNAL void errors_add_errorf(errors_t *errors, error_type_t type, src_pos_t pos, const char *format, ...) __attribute__ ((format (printf, 4, 5)));
APE_INTERNAL void errors_clear(errors_t *errors);
APE_INTERNAL int errors_get_count(const errors_t *errors);
APE_INTERNAL error_t *errors_get(errors_t *errors, int ix);
APE_INTERNAL const error_t *errors_getc(const errors_t *errors, int ix);
APE_INTERNAL const char *error_type_to_string(error_type_t type);
APE_INTERNAL error_t *errors_get_last_error(errors_t *errors);
APE_INTERNAL bool errors_has_errors(const errors_t *errors);

#endif /* errors_h */
