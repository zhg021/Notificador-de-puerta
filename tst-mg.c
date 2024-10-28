#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#define INTERRUPT_PIN GPIO_NUM_13

uint8_t one_time1 = 1;
uint8_t one_time2 = 1;

void set_pins();
void read_door();

void app_main(void) {
    
    set_pins();
    xTaskCreate(&read_door, "lectura_puerta", 1024, NULL, 5, NULL);
    
}

void set_pins(){
    gpio_reset_pin(INTERRUPT_PIN);
    //gpio_pad_select_gpio(INTERRUPT_PIN);
    gpio_set_direction(INTERRUPT_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(INTERRUPT_PIN, GPIO_PULLUP_ONLY);
}

void read_door(){
