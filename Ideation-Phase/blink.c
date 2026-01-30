/**
 * Example: Blink LED
 * 
 * This is a simple example of what user code might look like.
 * Save this to /examples/blink.c on the SD card.
 */

#include <pico/stdlib.h>

#define LED_PIN 25

int main() {
    // Initialize GPIO
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    
    // Blink forever
    while (1) {
        gpio_put(LED_PIN, 1);
        sleep_ms(500);
        gpio_put(LED_PIN, 0);
        sleep_ms(500);
    }
    
    return 0;
}
