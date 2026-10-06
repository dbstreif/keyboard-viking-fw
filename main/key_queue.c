#include <string.h>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/queue.h"
#include "portmacro.h"

#include "key_queue.h"

static QueueHandle_t key_queue;

esp_err_t key_queue_init(void) {
    key_queue = xQueueCreate(128, sizeof(key_log_event_t));
    if (key_queue == NULL) return ESP_ERR_INVALID_STATE;
    return ESP_OK;
}

void key_queue_send(key_log_event_t *key_event)
{
    xQueueSend(key_queue, (void *)key_event, 0);
}

bool key_queue_recv(key_log_event_t *key_event)
{
    return xQueueReceive(key_queue, key_event, portMAX_DELAY);
}

void key_queue_clear(void)
{
    xQueueReset(key_queue);
}
