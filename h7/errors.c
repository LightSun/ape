#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef APE_AMALGAMATED
#include "errors.h"
#include "traceback.h"
#endif

void errors_init(errors_t *errors) {
    memset(errors, 0, sizeof(errors_t));
    errors->count = 0;
}

void errors_deinit(errors_t *errors) {
    errors_clear(errors);
}

void errors_add_error(errors_t *errors, error_type_t type, src_pos_t pos, const char *message) {
    if (errors->count >= ERRORS_MAX_COUNT) {
        return;
    }
    error_t err;
    memset(&err, 0, sizeof(error_t));
    err.type = type;
    int len = (int)strlen(message);
    int to_copy = len;
    if (to_copy >= (ERROR_MESSAGE_MAX_LENGTH - 1)) {
        to_copy = ERROR_MESSAGE_MAX_LENGTH - 1;
    }
    memcpy(err.message, message, to_copy);
    err.message[to_copy] = '\0';
    err.pos = pos;
    err.traceback = NULL;
    errors->errors[errors->count] = err;
    errors->count++;
}

void errors_add_errorf(errors_t *errors, error_type_t type, src_pos_t pos, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int to_write = vsnprintf(NULL, 0, format, args);
    (void)to_write;
    va_end(args);
    va_start(args, format);
    char res[ERROR_MESSAGE_MAX_LENGTH];
    int written = vsnprintf(res, ERROR_MESSAGE_MAX_LENGTH, format, args);
    (void)written;
    APE_ASSERT(to_write == written);
    va_end(args);
    errors_add_error(errors, type, pos, res);
}

void errors_clear(errors_t *errors) {
    for (int i = 0; i < errors_get_count(errors); i++) {
        error_t *error = errors_get(errors, i);
        if (error->traceback) {
            traceback_destroy(error->traceback);
        }
    }
    errors->count = 0;
}

int errors_get_count(const errors_t *errors) {
    return errors->count;
}

error_t *errors_get(errors_t *errors, int ix) {
    if (ix >= errors->count) {
        return NULL;
    }
    return &errors->errors[ix];
}

const error_t *errors_getc(const errors_t *errors, int ix) {
    if (ix >= errors->count) {
        return NULL;
    }
    return &errors->errors[ix];
}

const char *error_type_to_string(error_type_t type) {
    switch (type) {
        case ERROR_PARSING:     return "PARSING";
        case ERROR_COMPILATION: return "COMPILATION";
        case ERROR_RUNTIME:     return "RUNTIME";
        case ERROR_TIMEOUT:     return "TIMEOUT";
        case ERROR_ALLOCATION:  return "ALLOCATION";
        case ERROR_USER:        return "USER";
        default:                return "INVALID";
    }
}

error_t *errors_get_last_error(errors_t *errors) {
    if (errors->count <= 0) {
        return NULL;
    }
    return &errors->errors[errors->count - 1];
}

bool errors_has_errors(const errors_t *errors) {
    return errors_get_count(errors) > 0;
}
