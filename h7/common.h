#ifndef common_h
#define common_h

#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include <float.h>

#if defined(__linux__)
    #define APE_LINUX
    #define APE_POSIX
#elif defined(_WIN32)
    #define APE_WINDOWS
#elif (defined(__APPLE__) && defined(__MACH__))
    #define APE_APPLE
    #define APE_POSIX
#elif defined(__EMSCRIPTEN__)
    #define APE_EMSCRIPTEN
#endif

#if defined(__unix__)
#include <unistd.h>
#if defined(_POSIX_VERSION)
    #define APE_POSIX
#endif
#endif

#if defined(APE_POSIX)
#include <sys/time.h>
#elif defined(APE_WINDOWS)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(APE_EMSCRIPTEN)
#include <emscripten/emscripten.h>
#endif

#ifndef APE_AMALGAMATED
#include "ape.h"
#endif

#define APE_STREQ(a, b) (strcmp((a), (b)) == 0)
#define APE_STRNEQ(a, b, n) (strncmp((a), (b), (n)) == 0)
#define APE_ARRAY_LEN(array) ((int)(sizeof(array) / sizeof(array[0])))
#define APE_DBLEQ(a, b) (fabs((a) - (b)) < DBL_EPSILON)

#ifdef APE_DEBUG
    #define APE_ASSERT(x) assert((x))
    #define APE_FILENAME (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
    #define APE_LOG(...) ape_log(APE_FILENAME, __LINE__, __VA_ARGS__)
#else
    #define APE_ASSERT(x) ((void)0)
    #define APE_LOG(...) ((void)0)
#endif

#ifdef APE_AMALGAMATED
    #define COLLECTIONS_AMALGAMATED
    #define APE_INTERNAL static
#else
    #define APE_INTERNAL
#endif

typedef struct compiled_file compiled_file_t;
typedef struct allocator allocator_t;

typedef struct src_pos {
    const compiled_file_t *file;
    int line;
    int column;
} src_pos_t;

typedef struct ape_config {
    struct {
        struct {
            ape_stdout_write_fn write;
            void *context;
        } write;
    } stdio;

    struct {
        struct {
            ape_read_file_fn read_file;
            void *context;
        } read_file;

        struct {
            ape_write_file_fn write_file;
            void *context;
        } write_file;
    } fileio;

    bool repl_mode; // allows redefinition of symbols

    double max_execution_time_ms;
    bool max_execution_time_set;
} ape_config_t;

typedef struct ape_timer {
#if defined(APE_POSIX)
    int64_t start_offset;
#elif defined(APE_WINDOWS)
    double pc_frequency;
#endif
    double start_time_ms;
} ape_timer_t;

#ifndef APE_AMALGAMATED
extern const src_pos_t src_pos_invalid;
extern const src_pos_t src_pos_zero;
#endif

APE_INTERNAL src_pos_t src_pos_make(const compiled_file_t *file, int line, int column);
APE_INTERNAL char *ape_stringf(allocator_t *alloc, const char *format, ...);
APE_INTERNAL void ape_log(const char *file, int line, const char *format, ...);
APE_INTERNAL char* ape_strndup(allocator_t *alloc, const char *string, size_t n);
APE_INTERNAL char* ape_strdup(allocator_t *alloc, const char *string);

APE_INTERNAL uint64_t ape_double_to_uint64(double val);
APE_INTERNAL double ape_uint64_to_double(uint64_t val);

APE_INTERNAL bool ape_timer_platform_supported(void);
APE_INTERNAL ape_timer_t ape_timer_start(void);
APE_INTERNAL double ape_timer_get_elapsed_ms(const ape_timer_t *timer);

#endif /* common_h */
