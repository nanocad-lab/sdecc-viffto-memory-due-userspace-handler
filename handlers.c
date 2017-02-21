/**
 * Author: Mark Gottscho
 * Email: mgottscho@ucla.edu
 * 
 * Handler definitions for dealing with memory DUEs
 */ 

#include "handlers.h"
#include "memory_due.h"
#include "hello.h"
#include <stdio.h> //sprintf()
#include <string.h> //memcpy()

//Define relevant global data structures that are needed for DUE handlers at runtime
DECL_DUE_INFO(main, overall)
DECL_DUE_INFO(main, init)
DECL_DUE_INFO(main, compute)
DECL_RECOVERY(main,m)
DECL_RECOVERY(main,b)
DECL_RECOVERY(main,i)
DECL_RECOVERY(main,x)
DECL_RECOVERY(main,y)

//Define handler functions

int DUE_RECOVERY_HANDLER(main, overall, dueinfo_t *recovery_context) {
    /*********** These must come first for macros to work properly  ************/
    static unsigned invocations = 0;
    invocations++;
    COPY_DUE_INFO(main, overall, recovery_context)
    /***************************************************************************/

    /******************************* INIT **************************************/
    int retval = -1;
    sprintf(recovery_context->expl, "Unknown error scope");
    /***************************************************************************/
    
    //TODO: Can we write this using a switch-case style using macros that are easy to read?
    //TODO: how to deal with multiple variables per message? For example, two 32-bit ints packed into 64-bit message? Multiple cases below can fire, but we don't want them to.
    
    /********************** CORRECTNESS-CRITICAL -- FORCE CRASH ****************/
    if (DUE_IN(main, overall, x)) {
        DUE_IN_SPRINTF(main, overall, x, recovery_context->expl)
        retval = -1;
    }
    if (DUE_IN(main, overall, y)) {
        DUE_IN_SPRINTF(main, overall, y, recovery_context->expl)
        retval = -1;
    }
    if (DUE_IN(main, overall, m)) {
        DUE_IN_SPRINTF(main, overall, m, recovery_context->expl)
        retval = -1;
    }
    if (DUE_IN(main, overall, b)) {
        DUE_IN_SPRINTF(main, overall, b, recovery_context->expl)
        retval = -1;
    }
    if (DUE_IN(main, overall, i)) {
        DUE_IN_SPRINTF(main, overall, i, recovery_context->expl)
        retval = -1;
    }
    /***************************************************************************/

   
    /***** FULLY APPROXIMABLE VARIABLES -- FALL BACK TO OS-GUIDED RECOVERY *****/
    /***************************************************************************/

    
    /*************** APP-DEFINED CUSTOM RECOVERY FOR SPECIFIC CASES ************/
    /***************************************************************************/


    retval = -1;

    /********** Ensure state is properly committed before returning ************/
    COPY_DUE_INFO(main, overall, recovery_context)
    return retval;
    /***************************************************************************/
}

int DUE_RECOVERY_HANDLER(main, init, dueinfo_t *recovery_context) {
    /*********** These must come first for macros to work properly  ************/
    static unsigned invocations = 0;
    invocations++;
    COPY_DUE_INFO(main, init, recovery_context)
    /***************************************************************************/

    /******************************* INIT **************************************/
    int retval = -1;
    sprintf(recovery_context->expl, "Unknown error scope");
    /***************************************************************************/
    
    //TODO: Can we write this using a switch-case style using macros that are easy to read?
    //TODO: how to deal with multiple variables per message? For example, two 32-bit ints packed into 64-bit message? Multiple cases below can fire, but we don't want them to.
   
    /********************** CORRECTNESS-CRITICAL -- FORCE CRASH ****************/
    /***************************************************************************/

    /***** FULLY APPROXIMABLE VARIABLES -- FALL BACK TO OS-GUIDED RECOVERY *****/
    if (DUE_IN(main, init, x)) {
        DUE_IN_SPRINTF(main, init, x, recovery_context->expl)
        retval = 1;
    }
    if (DUE_IN(main, init, y)) {
        DUE_IN_SPRINTF(main, init, y, recovery_context->expl)
        retval = 1;
    }
    if (DUE_IN(main, init, m)) {
        DUE_IN_SPRINTF(main, init, m, recovery_context->expl)
        retval = 1;
    }
    if (DUE_IN(main, init, b)) {
        DUE_IN_SPRINTF(main, init, b, recovery_context->expl)
        retval = 1;
    }
    /***************************************************************************/
    
    /*************** APP-DEFINED CUSTOM RECOVERY FOR SPECIFIC CASES ************/
    //If error is in i, it's control flow and we need to be careful. It can still be heuristically recovered but needs bounds check.
    if (DUE_IN(main, init, i)) {
        DUE_IN_SPRINTF(main, init, i, recovery_context->expl)
        long c_i;
        for (unsigned i = 0; i < recovery_context->candidates.size; i++) {
            memcpy(&c_i, recovery_context->candidates.candidate_messages[i].bytes, 8); //FIXME: what about mismatched variable and message sizes?
            if (c_i > 0 && c_i <= ARRAY_SIZE) { //Check that this candidate produces a legal i value based on semantics of the code in which it appears. At least one candidate should match or there is a major issue.
                copy_word(&(recovery_context->recovered_message), recovery_context->candidates.candidate_messages+i);
                //Restart section just to be safe to make sure we initialize every iteration of the loop.
                g_handler_stack[g_handler_sp].restart = 1;
                recovery_context->setup.restart = 1;
                retval = 0;
                break;
            }
        }
    }
    /***************************************************************************/

    /********** Ensure state is properly committed before returning ************/
    COPY_DUE_INFO(main, init, recovery_context)
    return retval;
    /***************************************************************************/
}

int DUE_RECOVERY_HANDLER(main, compute, dueinfo_t *recovery_context) {
    /*********** These must come first for macros to work properly  ************/
    static unsigned invocations = 0;
    invocations++;
    COPY_DUE_INFO(main, compute, recovery_context)
    /***************************************************************************/

    /******************************* INIT **************************************/
    int retval = 1;
    sprintf(recovery_context->expl, "Unknown error scope");
    /***************************************************************************/
    
    //TODO: Can we write this using a switch-case style using macros that are easy to read?
    //TODO: how to deal with multiple variables per message? For example, two 32-bit ints packed into 64-bit message? Multiple cases below can fire, but we don't want them to.
    
    /********************** CORRECTNESS-CRITICAL -- FORCE CRASH ****************/
    /***************************************************************************/

   
    /***** FULLY APPROXIMABLE VARIABLES -- FALL BACK TO OS-GUIDED RECOVERY *****/
    if (DUE_IN(main, compute, x)) {
        DUE_IN_SPRINTF(main, compute, x, recovery_context->expl)
        retval = 1;
    }
    if (DUE_IN(main, compute, y)) {
        DUE_IN_SPRINTF(main, compute, y, recovery_context->expl)
        retval = 1;
    }
    if (DUE_IN(main, compute, m)) {
        DUE_IN_SPRINTF(main, compute, m, recovery_context->expl)
        retval = 1;
    }
    if (DUE_IN(main, compute, b)) {
        DUE_IN_SPRINTF(main, compute, b, recovery_context->expl)
        retval = 1;
    }
    if (DUE_IN(main, compute, i)) {
        DUE_IN_SPRINTF(main, compute, i, recovery_context->expl)
        retval = 1;
    }
    /***************************************************************************/

    
    /*************** APP-DEFINED CUSTOM RECOVERY FOR SPECIFIC CASES ************/
    /***************************************************************************/


    /********** Ensure state is properly committed before returning ************/
    COPY_DUE_INFO(main, compute, recovery_context)
    return retval;
    /***************************************************************************/
}
