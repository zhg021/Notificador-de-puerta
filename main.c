#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "freertos/event_groups.h"
#include "esp_system.h"
#include <sys/param.h>
#include "lwip/sockets.h"
#include "lwip/inet.h"
#include "driver/gpio.h"
#include "driver/adc.h"  // Necesario para la funcionalidad de ADC

#define led GPIO_NUM_13
#define EXAMPLE_ESP_WIFI_SSID      "" //nombre de tu red
#define EXAMPLE_ESP_WIFI_PASS      "" //clave wifi
#define EXAMPLE_ESP_MAXIMUM_RETRY  5 //max intentos

#define server_ip "192.168.1.77"
#define PORT 19972
#define BUF_SIZE 1024
static const char *TAG2 = "UDP_SERVER";

#define EXAMPLE_ESP_WIFI_SSID      CONFIG_ESP_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASS      CONFIG_ESP_WIFI_PASSWORD
#define EXAMPLE_ESP_WIFI_CHANNEL   CONFIG_ESP_WIFI_CHANNEL
#define EXAMPLE_MAX_STA_CONN       CONFIG_ESP_MAX_STA_CONN

static EventGroupHandle_t s_wifi_event_group;

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
static const char *TAG = "wifi softAP";
static int s_retry_num = 0;

static const char *TAG0 = "wifi station";


#if CONFIG_ESP_WPA3_SAE_PWE_HUNT_AND_PECK
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_HUNT_AND_PECK
#define EXAMPLE_H2E_IDENTIFIER ""
#elif CONFIG_ESP_WPA3_SAE_PWE_HASH_TO_ELEMENT
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_HASH_TO_ELEMENT
#define EXAMPLE_H2E_IDENTIFIER CONFIG_ESP_WIFI_PW_ID
#elif CONFIG_ESP_WPA3_SAE_PWE_BOTH
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_BOTH
#define EXAMPLE_H2E_IDENTIFIER CONFIG_ESP_WIFI_PW_ID
#endif
#if CONFIG_ESP_WIFI_AUTH_OPEN
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_OPEN
#elif CONFIG_ESP_WIFI_AUTH_WEP
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WEP
#elif CONFIG_ESP_WIFI_AUTH_WPA_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WAPI_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WAPI_PSK
#endif

void init_led(){

  gpio_reset_pin(led);
  gpio_set_direction(led, GPIO_MODE_OUTPUT);

}

static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
   
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG0, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG0,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG0, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_sta(void) {

    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .password = EXAMPLE_ESP_WIFI_PASS,
            /* Authmode threshold resets to WPA2 as default if password matches WPA2 standards (password len => 8).
             * If you want to connect the device to deprecated WEP/WPA networks, Please set the threshold value
             * to WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK and set the password with length and format matching to
             * WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK standards.
             */
            .threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,
            .sae_pwe_h2e = ESP_WIFI_SAE_MODE,
            .sae_h2e_identifier = EXAMPLE_H2E_IDENTIFIER,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG0, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG0, "connected to ap SSID:%s password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG0, "Failed to connect to SSID:%s, password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else {
        ESP_LOGE(TAG0, "UNEXPECTED EVENT");
    }
}

void udp_server5(void *pvParameters) {

    char rx_buffer[BUF_SIZE];
    char addr_str[128];
    int addr_family = AF_INET;
    int ip_protocol = IPPROTO_UDP;

    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = inet_addr(server_ip);  // Dirección IP del servidor
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(PORT);

    int sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
    if (sock < 0) {
        ESP_LOGE(TAG2, "Unable to create socket: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG2, "Socket created");

    int err = bind(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err < 0) {
        ESP_LOGE(TAG2, "Socket unable to bind: errno %d", errno);
        close(sock);
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG2, "Socket bound, port %d", PORT);

    // Configuración del ADC
    adc1_config_width(ADC_WIDTH_BIT_12);  // Configurar el ADC para una resolución de 12 bits
    adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_DB_11);  // Configurar la atenuación (0dB, 2.5dB, 6dB, 11dB)

    while (1) {
        ESP_LOGI(TAG2, "Waiting for data");
        struct sockaddr_in source_addr;
        socklen_t socklen = sizeof(source_addr);
        int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&source_addr, &socklen);

        if (len < 0) {
            ESP_LOGE(TAG2, "recvfrom failed: errno %d", errno);
            break;
        } else {
            rx_buffer[len] = 0;  // Null-terminating the received string
            inet_ntoa_r(source_addr.sin_addr, addr_str, sizeof(addr_str) - 1);
            ESP_LOGI(TAG2, "Received %d bytes from %s:", len, addr_str);
            ESP_LOGI(TAG2, "%s", rx_buffer);

            // Parse the command format: UABC:ZYH:<operation>:<recurso>:<comentario>
            char *token = strtok(rx_buffer, ":");
            if (token && strcmp(token, "UABC") == 0) {
                char *zyh_prefix = strtok(NULL, ":");
                char *operation = strtok(NULL, ":");
                char *recurso = strtok(NULL, ":");
                char *comment = strtok(NULL, ":");

                if (zyh_prefix && strcmp(zyh_prefix, "ZYH") == 0 && operation && recurso) {
                    if (strcmp(recurso, "L") == 0) {  // Recurso es LED
                        if (strcmp(operation, "W") == 0) {  // Operación de escritura
                            int led_value = atoi(comment);  // Valor del LED (1 ó 0)
                            if (led_value == 0 || led_value == 1) {
                                gpio_set_level(led, led_value);  // Controlar LED
                                char response[BUF_SIZE];
                                snprintf(response, sizeof(response), "ACK:L:%d", led_value);
                                err = sendto(sock, response, strlen(response), 0, (struct sockaddr *)&source_addr, sizeof(source_addr));
                                ESP_LOGI(TAG2, "LED set to %d", led_value);
                            } else {
                                const char *response = "NACK";
                                sendto(sock, response, strlen(response), 0, (struct sockaddr *)&source_addr, sizeof(source_addr));
                                ESP_LOGE(TAG2, "Invalid LED value");
                            }
                        } else if (strcmp(operation, "R") == 0) {  // Operación de lectura
                            int current_led_value = gpio_get_level(led);  // Estado actual del LED
                            char response[BUF_SIZE];
                            snprintf(response, sizeof(response), "ACK:L:%d", current_led_value);
                            err = sendto(sock, response, strlen(response), 0, (struct sockaddr *)&source_addr, sizeof(source_addr));
                            ESP_LOGI(TAG2, "LED state read: %d", current_led_value);
                        } else {
                            const char *response = "NACK";
                            sendto(sock, response, strlen(response), 0, (struct sockaddr *)&source_addr, sizeof(source_addr));
                            ESP_LOGE(TAG2, "Invalid command for LED");
                        }
                    } else if (strcmp(recurso, "A") == 0 && strcmp(operation, "R") == 0) {  // Recurso es ADC y operación es lectura
                        // Leer valor del ADC del pin GPIO 36 (ADC1_CHANNEL_0)
                        int adc_value = adc1_get_raw(ADC1_CHANNEL_0);
                        char response[BUF_SIZE];
                        snprintf(response, sizeof(response), "ACK:A:%d", adc_value);
                        err = sendto(sock, response, strlen(response), 0, (struct sockaddr *)&source_addr, sizeof(source_addr));
                        ESP_LOGI(TAG2, "ADC value read: %d", adc_value);
                    } else {
                        const char *response = "NACK";
                        sendto(sock, response, strlen(response), 0, (struct sockaddr *)&source_addr, sizeof(source_addr));
                        ESP_LOGE(TAG2, "Invalid resource or operation");
                    }
                } else {
                    const char *response = "NACK";
                    sendto(sock, response, strlen(response), 0, (struct sockaddr *)&source_addr, sizeof(source_addr));
                    ESP_LOGE(TAG2, "Invalid command format");
                }
            } else {
                const char *response = "NACK";
                sendto(sock, response, strlen(response), 0, (struct sockaddr *)&source_addr, sizeof(source_addr));
                ESP_LOGE(TAG2, "Invalid command prefix");
            }
        }
    }

    if (sock != -1) {
        ESP_LOGE(TAG2, "Shutting down socket and restarting...");
        close(sock);
    }
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {

    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d, reason=%d",
                 MAC2STR(event->mac), event->aid, event->reason);
    }
}

void wifi_init_softap(void) {

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .ssid_len = strlen(EXAMPLE_ESP_WIFI_SSID),
            .channel = EXAMPLE_ESP_WIFI_CHANNEL,
            .password = EXAMPLE_ESP_WIFI_PASS,
            .max_connection = EXAMPLE_MAX_STA_CONN,
#ifdef CONFIG_ESP_WIFI_SOFTAP_SAE_SUPPORT
            .authmode = WIFI_AUTH_WPA3_PSK,
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
#else /* CONFIG_ESP_WIFI_SOFTAP_SAE_SUPPORT */
            .authmode = WIFI_AUTH_WPA2_PSK,
#endif
            .pmf_cfg = {
                    .required = true,
            },
        },
    };
    if (strlen(EXAMPLE_ESP_WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
             EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS, EXAMPLE_ESP_WIFI_CHANNEL);
}


void app_main(void) {
     init_led(); 

    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "ESP_WIFI_MODE_AP");
    wifi_init_softap();

    xTaskCreate(udp_server5, "udp_server", 4096, NULL, 5, NULL);
}
