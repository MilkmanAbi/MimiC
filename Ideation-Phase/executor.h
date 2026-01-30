/**
 * Runtime Executor
 * 
 * Executes compiled user code safely within FreeRTOS context
 */

#ifndef EXECUTOR_H
#define EXECUTOR_H

#include "../compiler/tcc_port.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Execute compiled code
 * 
 * @param result Compilation result containing executable code
 * @return Exit code from user's main()
 */
int executor_run(const CompileResult *result);

/**
 * Execute compiled code as FreeRTOS task
 * 
 * @param result Compilation result
 * @param task_name Name for the task
 * @param stack_size Stack size in words
 * @param priority Task priority
 * @return 0 on success, -1 on failure
 */
int executor_run_as_task(const CompileResult *result,
                         const char *task_name,
                         uint16_t stack_size,
                         uint32_t priority);

/**
 * Stop currently running user code
 */
void executor_stop(void);

/**
 * Check if user code is currently running
 */
int executor_is_running(void);

#ifdef __cplusplus
}
#endif

#endif /* EXECUTOR_H */
