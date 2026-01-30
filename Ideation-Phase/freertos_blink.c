/**
 * Example: FreeRTOS Task
 * 
 * Example showing how to use FreeRTOS from user code.
 * Save this to /examples/freertos_blink.c on the SD card.
 */

#include <pico/stdlib.h>
#include <FreeRTOS.h>
#include <task.h>

#define LED_PIN 25

void blink_task(void *params) {
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    
    while (1) {
        gpio_put(LED_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(500));
        gpio_put(LED_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

int main() {
    stdio_init_all();
    
    // Create task
    xTaskCreate(blink_task, "Blink", 256, NULL, 1, NULL);
    
    // Start scheduler
    vTaskStartScheduler();
    
    // Should never reach here
    while (1) {
        sleep_ms(1000);
    }
    
    return 0;
}
