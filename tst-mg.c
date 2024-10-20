#include <stdio.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// definir el GPIO donde esta conectado el sensor 3144E
#define HALL_SENSOR_PIN GPIO_NUM_4
// definir el GPIO para un LED (opcional)
#define LED_PIN GPIO_NUM_2

void app_main(void)
{
    // configuracion del GPIO para el sensor de efecto Hall
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;   // no usar interrupciones
    io_conf.mode = GPIO_MODE_INPUT;          // establecer como entrada
    io_conf.pin_bit_mask = (1ULL << HALL_SENSOR_PIN); // pin del sensor
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE; // deshabilitar pull-down
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;   // habilitar pull-up
    gpio_config(&io_conf);

    // configuracion del GPIO para el LED (opcional)
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);

    while (1) {
        // leer el valor del sensor
        int hall_sensor_value = gpio_get_level(HALL_SENSOR_PIN);

        // comprobar si el sensor detecta un campo magnetico
        if (hall_sensor_value == 0) {
            printf("Campo magnetico detectado\n");
            gpio_set_level(LED_PIN, 1); // encender LED
        } else {
            printf("No se detecta campo magnetico\n");
            gpio_set_level(LED_PIN, 0); // apagar LED
        }

        // esperar 500 ms antes de leer de nuevo
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
