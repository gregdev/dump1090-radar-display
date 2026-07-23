#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_psram.h"
#include "esp_heap_caps.h"

void app_main(void)
{
    for (int i = 0; i < 10; i++) {
        printf("psram_initialized=%d  psram_size=%u  free_spiram=%u\n",
               (int)esp_psram_is_initialized(),
               (unsigned)esp_psram_get_size(),
               (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
