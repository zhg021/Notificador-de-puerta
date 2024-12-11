#include <stdio.h>
#include <string.h> 
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/ledc.h"

#include "esp_system.h" 
#include "esp_wifi.h" 
#include "esp_log.h" 
#include "esp_event.h" 
#include "nvs_flash.h" 
#include "lwip/err.h" 
#include "lwip/sys.h" 

#include "lwip/sockets.h"

#define BUZZER_GPIO GPIO_NUM_4
#define INTERRUPT_PIN GPIO_NUM_13

//agregar var para cambiar entre ap/st
uint8_t interruptor = 0; // 0 = ap , 1 = st 

const char *ssid = "SSS"; // modificar cuando se integre ap 
const char *pass = "Yes110397"; // modificar cuando se integre ap 
int retry_num=5;

uint8_t one_time1 = 1;
uint8_t one_time2 = 1;

const char *tcp_server_ip = "201.170.217.124"; // IP del servidor TCP
const int tcp_server_port = 1997;            // Puerto del servidor TCP

const char *cmd = "cmd";
const char *email = "zhgale997@gmail.com"; // modificar cuando se integre ap 
const char *ubicacion = "sala1"; // modificar cuando se integre ap 
const char *estado_abierto = "Open";
const char *estado_cerrado = "Close";

void set_pins();
void read_door();
void buzz();
void stop_buzz();
void send_tcp_message(const char *estado);

static void wifi_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id,void *event_data){
    if(event_id == WIFI_EVENT_STA_START)
    {
    printf("WIFI CONNECTING....\n");
    }
    else if (event_id == WIFI_EVENT_STA_CONNECTED)
    {
    printf("WiFi CONNECTED\n");
    }
    else if (event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
    printf("WiFi lost connection\n");
    if(retry_num<5){esp_wifi_connect();retry_num++;printf("Retrying to Connect...\n");}
    }
    else if (event_id == IP_EVENT_STA_GOT_IP)
    {
    printf("Wifi got IP...\n\n");
    }
}

void wifi_connection(){
     //                          s1.4
    // 2 - Wi-Fi Configuration Phase
    esp_netif_init();
    esp_event_loop_create_default();     // event loop                    s1.2
    esp_netif_create_default_wifi_sta(); // WiFi station                      s1.3
    wifi_init_config_t wifi_initiation = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&wifi_initiation); //    
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL);
    wifi_config_t wifi_configuration = {
        .sta = {
            .ssid = "",
            .password = "",
           
           }
   
        };
    strcpy((char*)wifi_configuration.sta.ssid, ssid);
    strcpy((char*)wifi_configuration.sta.password, pass);    
    //esp_log_write(ESP_LOG_INFO, "Kconfig", "SSID=%s, PASS=%s", ssid, pass);
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_configuration);
    // 3 - Wi-Fi Start Phase
    esp_wifi_start();
    esp_wifi_set_mode(WIFI_MODE_STA);
    // 4- Wi-Fi Connect Phase
    esp_wifi_connect();
    printf( "wifi_init_softap finished. SSID:%s  password:%s",ssid,pass);
   
}

void app_main(void) {
    
    set_pins();
    nvs_flash_init();
    wifi_connection();

    xTaskCreate(&read_door, "lectura_puerta", 2048, NULL, 5, NULL);
    
}

void set_pins(){
    gpio_reset_pin(INTERRUPT_PIN);
    //gpio_pad_select_gpio(INTERRUPT_PIN);
    gpio_set_direction(INTERRUPT_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(INTERRUPT_PIN, GPIO_PULLUP_ONLY);
}

void read_door(){
    while (1) {
        int switch_state = gpio_get_level(INTERRUPT_PIN);
        if (switch_state == 1) {
            if (one_time1 == 1)
            {
                printf("Interruptor cerrado\n");
                one_time1 = 0;
                one_time2 = 1;
                send_tcp_message(estado_cerrado);  // Enviar estado cerrado / modificar para realizar smtp 
            }
            stop_buzz();
        } else {
            if (one_time2 == 1) {
                printf("Interruptor abierto\n");
                one_time2 = 0;
                one_time1 = 1;
                //envia tcp
                send_tcp_message(estado_abierto);  // Enviar estado cerrado / modificar para realizar smtp 
                
                
            }
            //parlante
            //sonido molest0
            buzz();
        }
        vTaskDelay(500 / portTICK_PERIOD_MS); // wait N ms 
    }
}

void buzz(){
  // Configuración del temporizador LEDC
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_LOW_SPEED_MODE,
        .duty_resolution  = LEDC_TIMER_10_BIT,
        .timer_num        = LEDC_TIMER_0,
        .freq_hz          = 1000,  // Frecuencia de 2 kHz
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);

    // Configuración del canal LEDC
    ledc_channel_config_t ledc_channel = {
        .speed_mode     = LEDC_LOW_SPEED_MODE,
        .channel        = LEDC_CHANNEL_0,
        .timer_sel      = LEDC_TIMER_0,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = BUZZER_GPIO,
        .duty           = 206, // 50% duty cycle (2^10 / 2)
        .hpoint         = 0
    };
    ledc_channel_config(&ledc_channel);
}

void stop_buzz() {
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

void send_tcp_message(const char *estado) {
    char message[128];
    snprintf(message, sizeof(message), "%s:%s:%s:%s", cmd, email, ubicacion, estado);

    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = inet_addr(tcp_server_ip);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(tcp_server_port);

    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (sock < 0) {
        printf("Unable to create socket: errno %d\n", errno);
        return;
    }

    int err = connect(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err != 0) {
        printf("Socket unable to connect: errno %d\n", errno);
        close(sock);
        return;
    }

    int len = send(sock, message, strlen(message), 0);
    if (len < 0) {
        printf("Error occurred during sending: errno %d\n", errno);
    } else {
        printf("Message sent: %s\n", message);
    }

    close(sock);
}

// funcion smtp gmail ?


