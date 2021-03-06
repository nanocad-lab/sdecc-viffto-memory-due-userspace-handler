/**
 * Author: Mark Gottscho
 * Email: mgottscho@ucla.edu
 */

#include "memory_due.h"
#include "minipk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

due_handler_t g_handler_stack[MAX_REGISTERED_HANDLERS]; 
int g_handler_sp = -1;

void dump_dueinfo(dueinfo_t* dueinfo) {
    if (dueinfo && dueinfo->valid) {
        printf("\n");
        printf("************* A memory DUE was recovered! **********\n");
        printf("\n");
        printf("-------- Trap frame -------\n");
        dump_tf(&(dueinfo->tf));
        printf("---------------------------\n");
        printf("\n");
        printf("---- Float trap frame -----\n");
        dump_float_regs(&(dueinfo->float_tf));
        printf("---------------------------\n");
        printf("\n");
        printf("---- Candidate messages ---\n");
        dump_candidate_messages(&(dueinfo->candidates));
        printf("---------------------------\n");
        printf("\n");
        printf("------ Cacheline (SI) -----\n");
        dump_cacheline(&(dueinfo->cacheline));
        printf("---------------------------\n");
        printf("\n");
        printf("----------- Setup ---------\n");
        dump_setup(&(dueinfo->setup));
        printf("---------------------------\n");
        printf("\n");
        printf("---- Demand load info -----\n");
        if (dueinfo->mem_type == 0 && dueinfo->float_regfile == 1) {
            printf("Demand load type: floating-point\n");
            printf("Demand load destination register: %s\n", g_float_regnames[dueinfo->load_dest_reg]);
        } else if (dueinfo->mem_type == 0 && dueinfo->float_regfile == 0) {
            printf("Demand load type: integer\n");
            printf("Demand load destination register: %s\n", g_int_regnames[dueinfo->load_dest_reg]);
        } else { //Inst fetch
            printf("Demand load type: instruction fetch\n");
        }
        printf("Demand load width: %lu\n", dueinfo->recovered_load_value.size);
        printf("---------------------------\n");
        printf("\n");
        printf("----- Error location ------\n");
        printf("Memory region type: %s\n", (dueinfo->mem_type == 0 ? "data" : "instruction"));
        printf("Victim message virtual address: %p\n", (void*)(dueinfo->tf.badvaddr));
        printf("Demand load virtual address: %p\n", (void*)(dueinfo->demand_vaddr));
        printf("Demand load-to-message offset: %d\n", dueinfo->load_message_offset);
        printf("The error is in the: ");
        if (dueinfo->error_in_stack)
            printf("stack ");
        if (dueinfo->error_in_text)
            printf("text-segment ");
        if (dueinfo->error_in_data)
            printf("data-segment ");
        if (dueinfo->error_in_sdata)
            printf("sdata-segment ");
        if (dueinfo->error_in_bss)
            printf("bss-segment ");
        if (dueinfo->error_in_heap)
            printf("heap ");
        printf("\n");
        if ((void*)(dueinfo->tf.epc) < dueinfo->setup.pc_start || (void*)(dueinfo->tf.epc) > dueinfo->setup.pc_end)
            printf("The DUE appears to have occurred in a subroutine.\n");
        printf("---------------------------\n");
        printf("\n");
        printf("----- Recovered data ------\n");
        printf("Recovered victim message: 0x");
        dump_word(&(dueinfo->recovered_message));
        printf("\n");
        printf("Victim message width: %lu\n", dueinfo->recovered_message.size);
        printf("Recovered demand load: 0x");
        dump_word(&(dueinfo->recovered_load_value));
        if (dueinfo->load_message_offset + dueinfo->load_size < 0 || dueinfo->load_message_offset >= dueinfo->recovered_message.size)
            printf(" (no victim message overlap, it should be uncorrupted)");
        printf("\n");
        dump_load_value(&(dueinfo->recovered_load_value), dueinfo->type_name);
        switch (dueinfo->recovery_mode) {
            case 0:
                printf("USER-specified recovery mode.\n");
                break;
            case 1:
                printf("SYSTEM-specified recovery mode.\n");
                break;
            default:
                printf("OPT-TO-CRASH recovery mode.\n"); //This should never actually get printed, LOL
                break;
        }
        printf("---------------------------\n");
        printf("\n");
        printf("----- DUE explanation -----\n");
        printf("%s", dueinfo->expl);
        printf("---------------------------\n");
    } else
        printf("No valid DUE info.\n");
}

void push_user_memory_due_trap_handler(const char* name, user_defined_trap_handler fptr, void* pc_start, void* pc_end, due_region_strictness_t strict) {
    //TODO FIXME: How to deal with memory errors in this function? It happens somewhat often..
    //TODO FIXME: memory barriers, atomicity, etc
    if (g_handler_sp+1 >= MAX_REGISTERED_HANDLERS) {
        printf("Failed to push new DUE handler, MAX_REGISTERED_HANDLERS has been exceeded.\n");
        return;
    }
    if (g_handler_sp+1 < 0) {
        printf("Failed to push new DUE handler, g_handler_sp == %d\n", g_handler_sp);
        return;
    }

    //Save necessary global user state
    memcpy(g_handler_stack[g_handler_sp+1].name, name, NAME_SIZE);
    g_handler_stack[g_handler_sp+1].fptr = fptr;
    g_handler_stack[g_handler_sp+1].strict = strict;
    g_handler_stack[g_handler_sp+1].pc_start = pc_start;
    g_handler_stack[g_handler_sp+1].pc_end = pc_end;
    g_handler_stack[g_handler_sp+1].restart = 0; //Set decision should be made by user at time of DUE

    //First invocation only
    static int init = 0;
    if (!init) {
        user_trap_handler entry_trap_fptr = &memory_due_handler_entry;
        asm volatile("or a0, zero, %0;" //Load default entry trap handler fptr into register a0
                     "li a7, 447;" //Load syscall number 447 (SYS_register_user_memory_due_trap_handler) into register a7
                     "ecall;" //Make RISC-V environment call to register our user-defined trap handler
                     :
                     : "r" (entry_trap_fptr));
        init = 1;
    }
    g_handler_sp++;
}

void pop_user_memory_due_trap_handler() {
    //TODO FIXME: How to deal with memory errors in this function? It happens somewhat often..
    //TODO FIXME: memory barriers, atomicity, etc
    if (g_handler_sp < 0) {
        printf("Failed to pop DUE handler stack, none are currently registered.\n");
        return;
    }

    //Save necessary global user state
    g_handler_sp--;
}

int memory_due_handler_entry(trapframe_t* tf, float_trapframe_t* float_tf, long demand_vaddr, due_candidates_t* candidates, due_cacheline_t* cacheline, word_t* recovered_message, size_t load_size, size_t load_dest_reg, int float_regfile, int load_message_offset, int mem_type) {
    if (g_handler_sp < 0 || g_handler_sp >= MAX_REGISTERED_HANDLERS) //probably our fault
        return -4;

    //TODO FIXME: How to deal with memory errors in this function? Re-entrant, etc.
    static dueinfo_t user_context; //Static because we don't want this allocated on the stack, it is a large data structure
    int success = 1;

    //Init to safe values: passed up as arguments from the OS
    user_context.valid = 0;
    user_context.tf.badvaddr = 0;
    user_context.tf.epc = 0;
    user_context.tf.insn = 0;
    user_context.tf.cause = 0;
    //user_context.float_tf //nothing to do
    user_context.demand_vaddr = 0;
    user_context.candidates.size = 0;
    user_context.cacheline.size = 0;
    user_context.recovered_message.size = 0;
    user_context.load_size = 0;
    user_context.load_dest_reg = 0;
    user_context.float_regfile = 0;
    user_context.load_message_offset = 0;
    user_context.mem_type = -1;

    //Init to safe values: set by userspace
    user_context.setup.name[0] = '\0';
    user_context.setup.fptr = NULL;
    user_context.setup.strict = 1;
    user_context.setup.pc_start = NULL;
    user_context.setup.pc_end = NULL;
    user_context.setup.restart = 0;
    user_context.setup.invocations = 0;
    user_context.setup.handler_sp_when_invoked = 0;
    user_context.recovered_load_value.size = 0;
    user_context.error_in_stack = 0;
    user_context.error_in_text = 0;
    user_context.error_in_data = 0;
    user_context.error_in_sdata = 0;
    user_context.error_in_bss = 0;
    user_context.error_in_heap = 0;
    user_context.recovery_mode = -1;
    user_context.type_name[0] = '\0';
    user_context.expl[0] = '\0';

    //Copy arguments from OS
    success = success & ((tf && copy_trapframe(&user_context.tf, tf) == 0) ? 1 : 0); 
    success = success & ((float_tf && copy_float_trapframe(&user_context.float_tf, float_tf) == 0) ? 1 : 0);
    user_context.demand_vaddr = demand_vaddr;
    success = success & ((candidates && copy_candidates(&user_context.candidates, candidates) == 0) ? 1 : 0);
    success = success & ((cacheline && copy_cacheline(&user_context.cacheline, cacheline) == 0) ? 1 : 0);
    success = success & ((copy_word(&user_context.recovered_message, recovered_message) == 0) ? 1 : 0);
    user_context.load_size = load_size;
    user_context.load_dest_reg = load_dest_reg;
    user_context.float_regfile = float_regfile;
    user_context.load_message_offset = load_message_offset;
    user_context.mem_type = mem_type;

    //Copy DUE handler setup context
    memcpy(user_context.setup.name, g_handler_stack[g_handler_sp].name, NAME_SIZE-1);
    user_context.setup.name[NAME_SIZE-1] = '\0';
    user_context.setup.fptr = g_handler_stack[g_handler_sp].fptr;
    user_context.setup.strict = g_handler_stack[g_handler_sp].strict;
    user_context.setup.pc_start = g_handler_stack[g_handler_sp].pc_start;
    user_context.setup.pc_end = g_handler_stack[g_handler_sp].pc_end;
    user_context.setup.restart = g_handler_stack[g_handler_sp].restart;
    //user_context.setup.invocations //nothing to do, set by user-defined handler
    user_context.setup.handler_sp_when_invoked = g_handler_sp;

    //Check arguments for correctness
    success = success & ((user_context.mem_type == 0 || user_context.mem_type == 1) ? 1 : 0);
    success = success & ((user_context.load_size <= sizeof(unsigned long)) ? 1 : 0);
    success = success & ((user_context.float_regfile == 0 || user_context.float_regfile == 1) ? 1 : 0);
    success = success & (((user_context.float_regfile == 0 && user_context.load_dest_reg <= NUM_GPR) || (user_context.float_regfile == 1 && user_context.load_dest_reg <= NUM_FPR)) ? 1 : 0);

    //Analyze trap frame, determine in which segment the memory DUE occured
    if (success) {
        void* badvaddr = (void*)(user_context.tf.badvaddr);
        if (badvaddr >= (void*)(user_context.tf.gpr[2]) && badvaddr < (void*)(user_context.tf.gpr[2]+64)) //gpr[2] is sp. TODO: how to find size of stack frame dynamically, or otherwise find the base of stack? Right now we look 0 to +64 bytes from the tf's sp (because it grows down)
            user_context.error_in_stack = 1;
        if (badvaddr >= (void*)(&_ftext) && badvaddr < (void*)(&_etext))
            user_context.error_in_text = 1;
        if (badvaddr >= (void*)(&_fdata) && badvaddr < (void*)(&_edata))
            user_context.error_in_data = 1;
        if (badvaddr >= (void*)(&_edata) && badvaddr < (void*)(&_fbss))
            user_context.error_in_sdata = 1;
        if (badvaddr >= (void*)(&_fbss) && badvaddr < (void*)(&_end))
            user_context.error_in_bss = 1;
        user_context.error_in_heap = 0; //TODO
    }

    user_context.valid = success;
    
    //Call user handler if we are not in strict mode or PC in error occurred in the registered PC range
    if (user_context.valid == 1) {
        user_defined_trap_handler fptr = user_context.setup.fptr;
        void* epc = (void*)(user_context.tf.epc);
        void* pc_start = (void*)(user_context.setup.pc_start);
        void* pc_end = (void*)(user_context.setup.pc_end);
        due_region_strictness_t strict = user_context.setup.strict;
        if (fptr) {
            if (strict == STRICTNESS_DEFAULT || (epc >= pc_start && epc < pc_end)) {
                user_context.recovery_mode = fptr(&user_context);
                copy_word(recovered_message, &(user_context.recovered_message));
                return user_context.recovery_mode;
            } else {
                return -3; //Out-of-bounds handler
            }   
        } else {
            //If we got here but fptr is NULL, then user did not successfully register handler..
            user_context.recovery_mode = -2; 
            return user_context.recovery_mode;
        }
    }

    //Handler problem, not app's fault
    user_context.recovery_mode = -4;
    return user_context.recovery_mode;
}

void dump_word(word_t* w) {
   for (size_t i = 0; i < w->size; i++)
       printf("%02x", w->bytes[i]);
}

void dump_candidate_messages(due_candidates_t* cd) {
   if (cd) {
       for (size_t i = 0; i < cd->size; i++) {
           printf("Candidate message %lu: 0x", i);
           dump_word(&(cd->candidate_messages[i]));
           printf("\n");
       }
   } else
       printf("Invalid candidate messages!\n");
}

void dump_cacheline(due_cacheline_t* cl) {
   if (cl) {
       for (size_t i = 0; i < cl->size; i++) {
           if (cl->blockpos != i) {
               printf("Word %lu: 0x", i);
               dump_word(&(cl->words[i]));
           } else
               printf("Word %lu: <CORRUPTED MESSAGE>", i);
           printf("\n");
       }
   } else
       printf("Invalid cacheline!\n");
}

void dump_setup(due_handler_t *setup) {
   printf("DUE handler name: %s\n", setup->name);
   printf("Handler invocations: %lu", setup->invocations);
   if (setup->invocations > 1)
       printf(" (ONLY REPORTING LAST INVOCATION)");
   printf("\n");
   printf("DUE handler stack depth when invoked: %d\n", setup->handler_sp_when_invoked);
   printf("DUE handler user function: %p\n", setup->fptr); 
   printf("DUE handling strictness: %s\n", (setup->strict ? "STRICT" : "DEFAULT")); 
   printf("DUE PC region start: %p\n", setup->pc_start);
   printf("DUE PC region end: %p\n", setup->pc_end);
   printf("DUE region restart: %d\n", setup->restart);
}

void dump_load_value(word_t* load, const char* type_name) {
    //TODO: Is it possible to get around strict aliasing warnings in code like below without relying on compiler-undefined behavior? Perhaps use memcpy()?
    size_t size = load->size;
    if (strcmp(type_name, "unsigned char") == 0 && size == sizeof(unsigned char)) {
        unsigned char val = (unsigned char)(*load->bytes);
        printf("Recovered load value (%s): %c\n", type_name, val);

    } else if (strcmp(type_name, "char") == 0 && size == sizeof(char)) {
        char val = (char)(*load->bytes);
        printf("Recovered load value (%s): %c\n", type_name, val);

    } else if (strcmp(type_name, "unsigned short") == 0 && size == sizeof(unsigned short)) {
        unsigned short val = (unsigned short)(*((unsigned short*)(load->bytes)));
        printf("Recovered load value (%s): %u\n", type_name, val);

    } else if (strcmp(type_name, "short") == 0 && size == sizeof(short)) {
        short val = (short)(*((short*)(load->bytes)));
        printf("Recovered load value (%s): %d\n", type_name, val);

    } else if (strcmp(type_name, "unsigned") == 0 && size == sizeof(unsigned)) {
        unsigned val = (unsigned)(*((unsigned*)(load->bytes)));
        printf("Recovered load value (%s): %u\n", type_name, val);

    } else if (strcmp(type_name, "int") == 0 && size == sizeof(int)) {
        int val = (int)(*((int*)(load->bytes)));
        printf("Recovered load value (%s): %d\n", type_name, val);

    } else if (strcmp(type_name, "unsigned long") == 0 && size == sizeof(unsigned long)) {
        unsigned long val = (unsigned long)(*((unsigned long*)(load->bytes)));
        printf("Recovered load value (%s): %lu\n", type_name, val);

    } else if (strcmp(type_name, "long") == 0 && size == sizeof(long)) {
        long val = (long)(*((long*)(load->bytes)));
        printf("Recovered load value (%s): %ld\n", type_name, val);

    } else if (strcmp(type_name, "unsigned long long") == 0 && size == sizeof(unsigned long long)) {
        unsigned long long val = (unsigned long long)(*((unsigned long long*)(load->bytes)));
        printf("Recovered load value (%s): %llu\n", type_name, val);

    } else if (strcmp(type_name, "long long") == 0 && size == sizeof(long long)) {
        long long val = (long long)(*((long long*)(load->bytes)));
        printf("Recovered load value (%s): %lld\n", type_name, val);

    } else if (strcmp(type_name, "void*") == 0 && size == sizeof(void*)) {
        void* val = (void*)((void*)(load->bytes));
        printf("Recovered load value (%s): %p\n", type_name, val);

    } else if (strcmp(type_name, "float") == 0 && size == sizeof(float)) {
        float val = (float)(*((float*)(load->bytes)));
        printf("Recovered load value (%s): %f\n", type_name, val);

    } else if (strcmp(type_name, "double") == 0 && size == sizeof(double)) {
        double val = (double)(*((double*)(load->bytes)));
        printf("Recovered load value (%s): %f\n", type_name, val);

    } else if (strcmp(type_name, "long double") == 0 && size == sizeof(long double)) {
        long double val = (long double)(*((long double*)(load->bytes)));
        printf("Recovered load value (%s): %Lf\n", type_name, val);

    } else {
        printf("Recovered load value (type %s, length %lu bytes)\n", type_name, size);
    }
}

void dump_float_regs(float_trapframe_t* float_tf) {
  for(size_t i = 0; i < NUM_FPR; i+=4)
  {
    for(size_t j = 0; j < 4; j++) {
        printf("%s %016lx%c",g_float_regnames[i+j],float_tf->fpr[i+j],j < 3 ? ' ' : '\n');
    }
  }
}

unsigned long get_sim_tick_counter() {
    unsigned long tick;
    asm volatile("csrr %0, 0xa" : "=r"(tick)); 
    return tick;
}

