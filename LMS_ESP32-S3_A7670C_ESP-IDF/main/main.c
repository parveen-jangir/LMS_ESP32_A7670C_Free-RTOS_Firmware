#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "main";

void app_main(void)
{
    while (1)
    {
        ESP_LOGI(TAG, "Application started");
        ESP_LOGE(TAG, "ERROR");
        ESP_LOGV(TAG, "VERBOSE");
        ESP_LOGD(TAG, "DEBUG");
        ESP_LOGW(TAG, "WARNING");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}