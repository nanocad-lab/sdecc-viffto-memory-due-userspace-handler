/**
 * Author: Mark Gottscho
 * Email: mgottscho@ucla.edu
 */

#ifndef MEMORY_DUE_H
#define MEMORY_DUE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>
#include <stdio.h>
#include "minipk.h"

#define NAME_SIZE 64
#define EXPL_SIZE 256
#define MAX_REGISTERED_HANDLERS 8

typedef enum {
    STRICTNESS_DEFAULT,
    STRICTNESS_STRICT,
    STRICTNESS_NUM
} due_region_strictness_t;

typedef struct due_handler due_handler_t; //Forward declaration for circular dependencies
typedef struct dueinfo dueinfo_t; //Forward declaration for circular dependencies

typedef int (*user_defined_trap_handler)(dueinfo_t*);

struct due_handler {
    char name[NAME_SIZE];
    user_defined_trap_handler fptr;
    due_region_strictness_t strict;
    void* pc_start;
    void* pc_end;
    int restart;
    unsigned long invocations;
    int handler_sp_when_invoked;
};

struct dueinfo {
    int valid;

    //Passed up as arguments from the OS
    trapframe_t tf;
    float_trapframe_t float_tf;
    long demand_vaddr;
    due_candidates_t candidates;
    due_cacheline_t cacheline;
    word_t recovered_message; //Can be modified by user code
    size_t load_size;
    size_t load_dest_reg;
    int float_regfile;
    int load_message_offset;
    int mem_type;

    //Set by userspace
    struct due_handler setup;
    word_t recovered_load_value;
    int error_in_stack;
    int error_in_text;
    int error_in_data;
    int error_in_sdata;
    int error_in_bss;
    int error_in_heap;
    int recovery_mode;
    char type_name[NAME_SIZE];
    char expl[EXPL_SIZE]; 
};

#define STR(x) #x
#define STRINGIFY(x) STR(x)
#define FILE_LINE __FILE__ "_" STRINGIFY(__LINE__)

#define VARIABLE_SCOPE_TYPE_NAME_PASTER(x,y) x ## _ ## y ## _type_name
#define VARIABLE_SCOPE_ADDR_PASTER(x,y) x ## _ ## y ## _addr
#define VARIABLE_SCOPE_ADDR_END_PASTER(x,y) x ## _ ## y ## _addr_end
#define FUNCTION_DUE_RECOVERY_NAME(fname, seqnum) fname ## _ ## seqnum ## _ ## memory_due_handler
#define DUE_RECOVERY_HANDLER(fname,seqnum,...) FUNCTION_DUE_RECOVERY_NAME(fname, seqnum)(__VA_ARGS__)

#define DECL_RECOVERY(scope, variable, type_name) \
    void* VARIABLE_SCOPE_ADDR_PASTER(scope, variable) = NULL; \
    void* VARIABLE_SCOPE_ADDR_END_PASTER(scope, variable) = NULL; \
    char VARIABLE_SCOPE_TYPE_NAME_PASTER(scope, variable)[NAME_SIZE] = #type_name; \

#define DECL_RECOVERY_EXTERN(scope, variable, type_name) \
    extern void* VARIABLE_SCOPE_ADDR_PASTER(scope, variable); \
    extern void* VARIABLE_SCOPE_ADDR_END_PASTER(scope, variable); \
    extern char VARIABLE_SCOPE_TYPE_NAME_PASTER(scope, variable)[NAME_SIZE]; \

#define EN_RECOVERY(scope, variable, size) \
    VARIABLE_SCOPE_ADDR_PASTER(scope, variable) = (void*)(&variable); \
    VARIABLE_SCOPE_ADDR_END_PASTER(scope, variable) = (void*)(&variable)+size;

#define EN_RECOVERY_PTR(scope, variable, size) \
    VARIABLE_SCOPE_ADDR_PASTER(scope, variable) = (void*)(variable); \
    VARIABLE_SCOPE_ADDR_END_PASTER(scope, variable) = (void*)(variable)+size;

#define RECOVERY_ADDR(scope, variable) \
    VARIABLE_SCOPE_ADDR_PASTER(scope, variable)

#define RECOVERY_END_ADDR(scope, variable) \
    VARIABLE_SCOPE_ADDR_END_PASTER(scope, variable)

#define START_DUE_REGION_LABEL(fname, seqnum) \
    fname ## _ ## seqnum ## _ ## start

#define END_DUE_REGION_LABEL(fname, seqnum) \
    fname ## _ ## seqnum ## _ ## end

#define BEGIN_DUE_RECOVERY(fname, seqnum, strict) \
    push_user_memory_due_trap_handler(STRINGIFY(FUNCTION_DUE_RECOVERY_NAME(fname, seqnum)), FUNCTION_DUE_RECOVERY_NAME(fname, seqnum), &&START_DUE_REGION_LABEL(fname, seqnum), &&END_DUE_REGION_LABEL(fname, seqnum), strict); \
    START_DUE_REGION_LABEL(fname,seqnum):;

#define END_DUE_RECOVERY(fname,seqnum) \
    END_DUE_REGION_LABEL(fname,seqnum):; \
    if (g_handler_stack[g_handler_sp].restart == 1) { \
        g_handler_stack[g_handler_sp].restart = 0; \
        printf("Restarting DUE trap region!\n"); \
        goto *(g_handler_stack[g_handler_sp].pc_start); \
    } \
    pop_user_memory_due_trap_handler();

#define DUE_INFO(fname, seqnum) fname ## _ ## seqnum ## _ ## dueinfo

#define DECL_DUE_INFO(fname, seqnum) \
    dueinfo_t DUE_INFO(fname, seqnum);

#define DECL_DUE_INFO_EXTERN(fname, seqnum) \
    extern dueinfo_t DUE_INFO(fname, seqnum);

#define COPY_DUE_INFO(fname, seqnum, src) \
    DUE_INFO(fname, seqnum).valid = 0; \
    if (src) { \
        int success = 1; \
        success = success & ((copy_trapframe(&(DUE_INFO(fname, seqnum).tf), &(src->tf)) == 0) ? 1 : 0); \
        success = success & ((copy_float_trapframe(&(DUE_INFO(fname, seqnum).float_tf), &(src->float_tf)) == 0) ? 1 : 0); \
        DUE_INFO(fname, seqnum).demand_vaddr = src->demand_vaddr; \
        success = success & ((copy_candidates(&(DUE_INFO(fname, seqnum).candidates), &(src->candidates)) == 0) ? 1 : 0); \
        success = success & ((copy_cacheline(&(DUE_INFO(fname, seqnum).cacheline), &(src->cacheline)) == 0)? 1 : 0); \
        success = success & ((copy_word(&(DUE_INFO(fname, seqnum).recovered_message), &(src->recovered_message)) == 0) ? 1 : 0); \
        DUE_INFO(fname, seqnum).load_size = src->load_size; \
        DUE_INFO(fname, seqnum).load_dest_reg = src->load_dest_reg; \
        DUE_INFO(fname, seqnum).float_regfile = src->float_regfile; \
        DUE_INFO(fname, seqnum).load_message_offset = src->load_message_offset; \
        DUE_INFO(fname, seqnum).mem_type = src->mem_type; \
        \
        memcpy(DUE_INFO(fname, seqnum).setup.name, src->setup.name, NAME_SIZE-1); \
        DUE_INFO(fname, seqnum).setup.name[NAME_SIZE-1] = '\0'; \
        DUE_INFO(fname, seqnum).setup.fptr = src->setup.fptr; \
        DUE_INFO(fname, seqnum).setup.strict = src->setup.strict; \
        DUE_INFO(fname, seqnum).setup.pc_start = src->setup.pc_start; \
        DUE_INFO(fname, seqnum).setup.pc_end = src->setup.pc_end; \
        DUE_INFO(fname, seqnum).setup.restart = src->setup.restart; \
        DUE_INFO(fname, seqnum).setup.invocations = invocations; \
        DUE_INFO(fname, seqnum).setup.handler_sp_when_invoked = src->setup.handler_sp_when_invoked; \
        \
        success = success & ((copy_word(&(DUE_INFO(fname, seqnum).recovered_load_value), &src->recovered_load_value) == 0) ? 1 : 0); \
        \
        DUE_INFO(fname, seqnum).error_in_stack = src->error_in_stack; \
        DUE_INFO(fname, seqnum).error_in_text = src->error_in_text; \
        DUE_INFO(fname, seqnum).error_in_data = src->error_in_data; \
        DUE_INFO(fname, seqnum).error_in_sdata = src->error_in_sdata; \
        DUE_INFO(fname, seqnum).error_in_bss = src->error_in_bss; \
        DUE_INFO(fname, seqnum).error_in_heap = src->error_in_heap; \
        DUE_INFO(fname, seqnum).recovery_mode = src->recovery_mode; \
        memcpy(DUE_INFO(fname, seqnum).type_name, src->type_name, NAME_SIZE-1); \
        DUE_INFO(fname, seqnum).type_name[NAME_SIZE-1] = '\0'; \
        memcpy(DUE_INFO(fname, seqnum).expl, src->expl, EXPL_SIZE-1); \
        DUE_INFO(fname, seqnum).expl[EXPL_SIZE-1] = '\0'; \
        \
        if (success == 1) \
            DUE_INFO(fname, seqnum).valid = 1; \
    }

#define DUE_IN(fname, seqnum, variable) \
    ((void *)(DUE_INFO(fname, seqnum).tf.badvaddr) >= RECOVERY_ADDR(fname, variable) && (void *)(DUE_INFO(fname, seqnum).tf.badvaddr) < RECOVERY_END_ADDR(fname, variable))

#define DEFAULT_DUE_SPRINTF(fname, seqnum, dueinfo) \
    sprintf(dueinfo->expl, "Unknown program context. DUE in %s(), PC %p, bad addr %p, type %s, var %s. Demand addr: %p, %lu bytes. Memory type: %s\n", #fname, (void*)(DUE_INFO(fname, seqnum).tf.epc), (void*)(DUE_INFO(fname, seqnum).tf.badvaddr), "<UNKNOWN>", "<UNKNOWN>", (void*)(DUE_INFO(fname, seqnum).demand_vaddr), DUE_INFO(fname, seqnum).load_size, (DUE_INFO(fname, seqnum).mem_type == 0 ? "data load" : "instruction fetch")); \
    sprintf(dueinfo->type_name, "%s", "<UNKNOWN>"); \

#define MULTIPLE_VARIABLES_DUE_SPRINTF(fname, seqnum, dueinfo) \
    sprintf(dueinfo->expl, "Multiple variables affected. DUE in %s(), PC %p, bad addr %p, type %s, var %s. Demand addr: %p, %lu bytes. Memory type: %s\n", #fname, (void*)(DUE_INFO(fname, seqnum).tf.epc), (void*)(DUE_INFO(fname, seqnum).tf.badvaddr), "<MULTIPLE>", "<MULTIPLE>", (void*)(DUE_INFO(fname, seqnum).demand_vaddr), DUE_INFO(fname, seqnum).load_size, (DUE_INFO(fname, seqnum).mem_type == 0 ? "data load" : "instruction fetch")); \
    sprintf(dueinfo->type_name, "%s", "<MULTIPLE>"); \

#define DUE_IN_SPRINTF(fname, seqnum, variable, type, dueinfo) \
    sprintf(dueinfo->expl, "DUE in %s(), PC %p, bad addr %p, type %s, var %s [%p, %p). Demand addr: %p, %lu bytes. Memory type: %s\n", #fname, (void*)(DUE_INFO(fname, seqnum).tf.epc), (void*)(DUE_INFO(fname, seqnum).tf.badvaddr), #type, #variable, RECOVERY_ADDR(fname, variable), RECOVERY_END_ADDR(fname, variable), (void*)(DUE_INFO(fname, seqnum).demand_vaddr), DUE_INFO(fname, seqnum).load_size, (DUE_INFO(fname, seqnum).mem_type == 0 ? "data load" : "instruction fetch")); \
    sprintf(dueinfo->type_name, "%s", #type); \

#define INJECT_DUE_INSTRUCTION(start_tick_offset, stop_tick_offset) \
    asm volatile("custom0 0,%0,%1,0;" \
                 : \
                 : "r" (start_tick_offset), "r" (stop_tick_offset));

#define INJECT_DUE_DATA(start_tick_offset, stop_tick_offset) \
    asm volatile("custom1 0,%0,%1,0;" \
                 : \
                 : "r" (start_tick_offset), "r" (stop_tick_offset));



//Useful symbols defined by the RISC-V linker script
extern void* _ftext; //Front of code segment
extern void* _etext; //End of code segment
extern void* _fdata; //Front of initialized data segment
extern void* _edata; //End of initialized data segment
extern void* _fbss; //Front of uninitialized data segment
extern void* _end; //End of uninitialized data segment... and address space overall?
extern due_handler_t g_handler_stack[MAX_REGISTERED_HANDLERS];
extern int g_handler_sp;

void dump_dueinfo(dueinfo_t* dueinfo);
void push_user_memory_due_trap_handler(const char* name, user_defined_trap_handler fptr, void* pc_start, void* pc_end, due_region_strictness_t strict);
void pop_user_memory_due_trap_handler();
int memory_due_handler_entry(trapframe_t* tf, float_trapframe_t* float_tf, long demand_vaddr, due_candidates_t* candidates, due_cacheline_t* cacheline, word_t* recovered_message, size_t load_size, size_t load_dest_reg, int float_regfile, int load_message_offset, int mem_type);
void dump_word(word_t* w);
void dump_candidate_messages(due_candidates_t* cd);
void dump_cacheline(due_cacheline_t* cl);
void dump_setup(due_handler_t *setup);
void dump_load_value(word_t* load, const char* type_name);
void dump_float_regs(float_trapframe_t* float_tf);
unsigned long get_sim_tick_counter();

#ifdef __cplusplus
} // extern "C"
#endif
#endif
