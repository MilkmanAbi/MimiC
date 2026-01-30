/**
 * UART Shell
 * 
 * Interactive command-line interface for controlling MimiC
 */

#ifndef UART_SHELL_H
#define UART_SHELL_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Shell task entry point
 * Runs as FreeRTOS task
 */
void shell_task(void *params);

#ifdef __cplusplus
}
#endif

#endif /* UART_SHELL_H */
