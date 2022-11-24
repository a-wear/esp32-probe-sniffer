#include <stdio.h>
#include <string.h>
#include "config.h"
#include "csi.h"
#include "freertos/FreeRTOS.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_check.h"

#define CSI_PROCESS_PACKET_TIMEOUT_MS   (100)

static const char *TAG = "csi";
static volatile bool stop = true;

typedef struct {
    bool is_running;
    TaskHandle_t task;
    QueueHandle_t work_queue;
    SemaphoreHandle_t sem_task_over;
} csi_runtime_t;


typedef struct {
    void *payload;
    uint32_t length;
    uint32_t seconds;
    uint32_t microseconds;
} csi_payload_info_t;

static csi_runtime_t csi_rt = {0};

FILE *file;

static esp_err_t csi_capture(void *payload, uint32_t length, uint32_t seconds, uint32_t microseconds)
{
    ESP_RETURN_ON_FALSE(payload, ESP_ERR_INVALID_ARG, TAG, "invalid argumnet");

    wifi_csi_info_t *csi_info = (wifi_csi_info_t *)payload;
    wifi_csi_info_t d = csi_info[0];
    
    // CSI data buffer
    char csi[1024];
    int index = 0;

    // Format Raw CSI data
    int8_t *my_ptr;
    my_ptr = csi_info->buf;
    for (int i = 0; i < csi_info->len; i++)
    {
        index += sprintf(&csi[index], "%d ", (int)my_ptr[i]);
    }

    fprintf(file, "%d,%d,%02X:%02X:%02X:%02X:%02X:%02X,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,[%s]\n",
            seconds,
            microseconds,
            d.mac[0], d.mac[1], d.mac[2], d.mac[3], d.mac[4], d.mac[5],
            d.rx_ctrl.rssi,
            d.rx_ctrl.rate,
            d.rx_ctrl.sig_mode,
            d.rx_ctrl.mcs,
            d.rx_ctrl.cwb,
            d.rx_ctrl.smoothing,
            d.rx_ctrl.not_sounding,
            d.rx_ctrl.aggregation,
            d.rx_ctrl.stbc,
            d.rx_ctrl.fec_coding,
            d.rx_ctrl.sgi,
            d.rx_ctrl.noise_floor,
            d.rx_ctrl.ampdu_cnt,
            d.rx_ctrl.channel,
            d.rx_ctrl.secondary_channel,
            d.rx_ctrl.timestamp,
            d.rx_ctrl.ant,
            d.rx_ctrl.sig_len,
            d.rx_ctrl.rx_state,
            csi_info->len,
            csi);

    return ESP_OK;
}

static void queue_csi(void *recv_packet, csi_payload_info_t *payload_info)
{
    if (payload_info->length > 172)
    {
        return;
    }

    void *payload_to_queue = malloc(payload_info->length);

    if(payload_to_queue)
    {
        memcpy(payload_to_queue, recv_packet, payload_info->length);
        payload_info->payload = payload_to_queue;

        if(csi_rt.work_queue)
        {            
            /* send payload info */
            if (xQueueSend(csi_rt.work_queue, payload_info, pdMS_TO_TICKS(CSI_PROCESS_PACKET_TIMEOUT_MS)) != pdTRUE)
            {
                ESP_LOGE(TAG, "csi work queue full");
                free(payload_info->payload);
            }
        }
    }
    else
    {
        ESP_LOGE(TAG, "Not enough memory for csi data");
    }  
}

void csi_sniffer_cb(void *ctx, wifi_csi_info_t *data) 
{
    struct timeval tv;
    csi_payload_info_t payload_info;

    gettimeofday(&tv, NULL);

    payload_info.seconds = tv.tv_sec;
    payload_info.microseconds = tv.tv_usec;
    payload_info.length = data->len + sizeof(wifi_csi_info_t);

    queue_csi(data, &payload_info);
}

static void csi_task(void *parameters)
{
    csi_payload_info_t payload_info;
    csi_runtime_t *csi = (csi_runtime_t *)parameters;

    while (csi->is_running)
    {
        /* receive packet info from queue */
        if (xQueueReceive(csi->work_queue, &payload_info, pdMS_TO_TICKS(CSI_PROCESS_PACKET_TIMEOUT_MS)) != pdTRUE)
        {
            continue;
        }
        if (csi_capture(payload_info.payload, payload_info.length, payload_info.seconds,
                           payload_info.microseconds) != ESP_OK)
        {
            ESP_LOGW(TAG, "save captured packet failed");
        }
        free(payload_info.payload);
    }

    /* notify that sniffer task is over */
    xSemaphoreGive(csi->sem_task_over);
    vTaskDelete(NULL);
}

esp_err_t csi_start(void)
{
    esp_err_t ret = ESP_OK;

    ESP_GOTO_ON_FALSE(!(csi_rt.is_running), ESP_ERR_INVALID_STATE, err, TAG, "csi is already running");

    csi_rt.is_running = true;
    csi_rt.work_queue = xQueueCreate(CONFIG_CSI_WORK_QUEUE_LEN, sizeof(csi_payload_info_t));
    ESP_GOTO_ON_FALSE(csi_rt.work_queue, ESP_FAIL, err_queue, TAG, "create work queue failed");
    csi_rt.sem_task_over = xSemaphoreCreateBinary();
    ESP_GOTO_ON_FALSE(csi_rt.sem_task_over, ESP_FAIL, err_sem, TAG, "create work queue failed");
    ESP_GOTO_ON_FALSE(xTaskCreate(csi_task, "csiT", CONFIG_CSI_TASK_STACK_SIZE,
                                  &csi_rt, CONFIG_CSI_TASK_PRIORITY, &csi_rt.task), ESP_FAIL,
                      err_task, TAG, "create task failed");
    return ret;

err_task:
    vSemaphoreDelete(csi_rt.sem_task_over);
    csi_rt.sem_task_over = NULL;
err_sem:
    vQueueDelete(csi_rt.work_queue);
    csi_rt.work_queue = NULL;
err_queue:
    csi_rt.is_running = false;
err:
    return ret;
}

esp_err_t csi_stop(void)
{
    esp_err_t ret = ESP_OK;

    ESP_GOTO_ON_FALSE(csi_rt.is_running, ESP_ERR_INVALID_STATE, err, TAG, "csi is already stopped");

    /* stop sniffer local task */
    csi_rt.is_running = false;
    /* wait for task over */
    xSemaphoreTake(csi_rt.sem_task_over, portMAX_DELAY);

    vSemaphoreDelete(csi_rt.sem_task_over);
    csi_rt.sem_task_over = NULL;
    /* make sure to free all resources in the left items */
    UBaseType_t left_items = uxQueueMessagesWaiting(csi_rt.work_queue);

    csi_payload_info_t payload_info;
    while (left_items--)
    {
        xQueueReceive(csi_rt.work_queue, &payload_info, pdMS_TO_TICKS(CSI_PROCESS_PACKET_TIMEOUT_MS));
        free(payload_info.payload);
    }
    vQueueDelete(csi_rt.work_queue);
    csi_rt.work_queue = NULL;
    
err:
    return ret;
}

void initialize_csi(void) 
{
    ESP_ERROR_CHECK(esp_wifi_set_csi(1));

    wifi_csi_config_t configuration_csi;
    configuration_csi.lltf_en = 1;
    configuration_csi.htltf_en = 1;
    configuration_csi.stbc_htltf2_en = 1;
    configuration_csi.ltf_merge_en = 1;
    configuration_csi.channel_filter_en = 0;
    configuration_csi.manu_scale = 0;

    ESP_ERROR_CHECK(esp_wifi_set_csi_config(&configuration_csi));
    ESP_ERROR_CHECK(esp_wifi_set_csi_rx_cb(&csi_sniffer_cb, NULL));
}

void write_csi_header(void)
{
    fprintf(file, "time_s,time_us,mac,rssi,rate,sig_mode,mcs,bandwidth,smoothing,not_sounding,aggregation,stbc,fec_coding,sgi,noise_floor,ampdu_cnt,channel,secondary_channel,local_timestamp,ant,sig_len,rx_state,len,CSI_DATA\n");
}

void open_csi_file(uint32_t idx)
{
    char filename[32];
    snprintf(filename, 32, CONFIG_SD_MOUNT_POINT"/"CONFIG_CSI_FILENAME_MASK, idx);
    file = fopen(filename, "w");
    write_csi_header();
}


void close_csi_file(void)
{
    fclose(file);
}