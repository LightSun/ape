#ifndef vm_h
#define vm_h

#ifndef APE_AMALGAMATED
#include "ape.h"
#include "common.h"
#include "ast.h"
#include "object.h"
#include "frame.h"
#include "errors.h"
#include "global_store.h"
#endif

#define VM_STACK_SIZE 2048
#define VM_MAX_GLOBALS 2048
#define VM_MAX_FRAMES 2048
#define VM_THIS_STACK_SIZE 2048

typedef struct ape_config ape_config_t;
typedef struct compilation_result compilation_result_t;

typedef struct vm {
    allocator_t *alloc;
    const ape_config_t *config;
    gcmem_t *mem;
    errors_t *errors;
    global_store_t *global_store;
    object_t globals[VM_MAX_GLOBALS];
    int globals_count;
    object_t stack[VM_STACK_SIZE];
    int sp;
    object_t this_stack[VM_THIS_STACK_SIZE];
    int this_sp;
    frame_t frames[VM_MAX_FRAMES];
    int frames_count;
    object_t last_popped;
    frame_t *current_frame;
    bool running;
    object_t operator_oveload_keys[OPCODE_MAX];
} vm_t;

APE_INTERNAL vm_t* vm_make(allocator_t *alloc, const ape_config_t *config, gcmem_t *mem, errors_t *errors, global_store_t *global_store); // config can be null (for internal testing purposes)
APE_INTERNAL void  vm_destroy(vm_t *vm);

APE_INTERNAL void vm_reset(vm_t *vm);

APE_INTERNAL bool vm_run(vm_t *vm, compilation_result_t *comp_res, array(object_t) *constants);
APE_INTERNAL object_t vm_call(vm_t *vm, array(object_t) *constants, object_t callee, int argc, object_t *args);
APE_INTERNAL bool vm_execute_function(vm_t *vm, object_t function, array(object_t) *constants);

APE_INTERNAL object_t vm_get_last_popped(vm_t *vm);
APE_INTERNAL bool vm_has_errors(vm_t *vm);

APE_INTERNAL bool vm_set_global(vm_t *vm, int ix, object_t val);
APE_INTERNAL object_t vm_get_global(vm_t *vm, int ix);

#endif /* vm_h */
