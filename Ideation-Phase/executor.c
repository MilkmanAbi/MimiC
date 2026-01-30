/**
 * Runtime Executor Implementation
 */

#include "executor.h"
#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"

static TaskHandle_t user_task_handle = NULL;

/**
 * Execute compiled code directly
 */
int executor_run(const CompileResult *result) {
    if (!result || !result->success || !result->entry_point) {
        printf("[Executor] Invalid compilation result\n");
        return -1;
    }
    
    printf("[Executor] Executing user code...\n");
    
    // Cast entry point to function pointer and call it
    typedef int (*main_func_t)(void);
    main_func_t main_func = (main_func_t)result->entry_point;
    
    int exit_code = main_func();
    
    printf("[Executor] User code exited with code %d\n", exit_code);
    
    return exit_code;
}

/**
 * Task wrapper for user code
 */
static void user_code_task(void *params) {
    const CompileResult *result = (const CompileResult *)params;
    
    printf("[Executor] User task started\n");
    
    // Execute user's main()
    int exit_code = executor_run(result);
    
    printf("[Executor] User task finished (exit code: %d)\n", exit_code);
    
    // Clean up
    user_task_handle = NULL;
    
    // Task deletes itself
    vTaskDelete(NULL);
}

/**
 * Execute as FreeRTOS task
 */
int executor_run_as_task(const CompileResult *result,
                         const char *task_name,
                         uint16_t stack_size,
                         uint32_t priority) {
    if (!result || !result->success || !result->entry_point) {
        printf("[Executor] Invalid compilation result\n");
        return -1;
    }
    
    if (user_task_handle != NULL) {
        printf("[Executor] User code already running\n");
        return -1;
    }
    
    printf("[Executor] Creating user task: %s\n", task_name);
    
    BaseType_t ret = xTaskCreate(
        user_code_task,
        task_name,
        stack_size,
        (void *)result,
        priority,
        &user_task_handle
    );
    
    if (ret != pdPASS) {
        printf("[Executor] Failed to create task\n");
        return -1;
    }
    
    return 0;
}

/**
 * Stop user code
 */
void executor_stop(void) {
    if (user_task_handle != NULL) {
        printf("[Executor] Stopping user code...\n");
        vTaskDelete(user_task_handle);
        user_task_handle = NULL;
    }
}

/**
 * Check if running
 */
int executor_is_running(void) {
    return (user_task_handle != NULL) ? 1 : 0;
}
