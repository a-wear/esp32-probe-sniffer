/* Includes ------------------------------------------------------------------*/
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>
#include "config.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "esp_sleep.h"
#include "nvs_flash.h"
#include "wifi_connect.h"
#include "esp_sntp.h"

/* Defines -------------------------------------------------------------------*/
#define ESP_INTR_FLAG_DEFAULT 0

/* Global variables-----------------------------------------------------------*/
static const char *TAG = "main";

static volatile bool stop_probing = false;
static xQueueHandle gpio_evt_queue = NULL;

/* Function prototypes -------------------------------------------------------*/
static void obtain_time(void);
static void initialize_gpio(void);
static void initialize_nvs(void);
static void initialize_sntp(void);

/* Interrupt service prototypes ----------------------------------------------*/
static void IRAM_ATTR gpio_isr_handler(void* arg);

/* Task prototypes -----------------------------------------------------------*/
static void gpio_task(void* arg);
/* Main function -------------------------------------------------------------*/
void app_main(void)
{
    time_t current_time;
    struct tm timeinfo;
    char strftime_buf[64];

    // Initialize peripherals and time
    initialize_gpio();
    initialize_nvs();
    obtain_time();

    // update 'current_time' variable with current time
    time(&current_time);

    // Set timezone
    setenv("TZ", "UTC-1", 1);
    tzset();
    localtime_r(&current_time, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(TAG, "The current date/time in Europe is: %s", strftime_buf);

    // Turn off LED when set up ends
    ESP_ERROR_CHECK(gpio_set_level(CONFIG_GPIO_LED_PIN, CONFIG_GPIO_LED_OFF));

    while(true)
    {
        if(stop_probing == true)
        {
            stop_probing = false;

            // Turn LED ON when SD unmounted
            ESP_ERROR_CHECK(gpio_set_level(CONFIG_GPIO_LED_PIN, CONFIG_GPIO_LED_ON));
            return;
        }
        vTaskDelay(10);
    }
}

/* Interrupt service routines ------------------------------------------------*/
static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

/* Task definitions ----------------------------------------------------------*/
static void gpio_task(void* arg)
{
    uint32_t io_num;

    while(true)
    {
        if(xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY))
        {
            stop_probing = true;
        }
    }
}


/* Function definitions ------------------------------------------------------*/
static void initialize_gpio(void)
{
    // LED PIN configuration
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << CONFIG_GPIO_LED_PIN);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    // Button interrupt configuration
    io_conf.intr_type = GPIO_INTR_POSEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << CONFIG_GPIO_BUTTON_PIN);
    io_conf.pull_up_en = 1;
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    //create a queue to handle gpio event from isr
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    //start gpio task
    xTaskCreate(gpio_task, "gpio_task", 2048, NULL, 10, NULL);

    //install gpio isr service
    ESP_ERROR_CHECK(gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT));
    //hook isr handler for specific gpio pin
    ESP_ERROR_CHECK(gpio_isr_handler_add(CONFIG_GPIO_BUTTON_PIN, gpio_isr_handler, (void*) CONFIG_GPIO_BUTTON_PIN));
}

static void initialize_nvs(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}

static void initialize_sntp(void)
{
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_init();
}

static void obtain_time(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_ERROR_CHECK(wifi_connect());

    initialize_sntp();

    // wait for time to be set
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET)
    {
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }

    ESP_ERROR_CHECK(wifi_disconnect() );
}
