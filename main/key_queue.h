#ifndef KEY_QUEUE_H
#define KEY_QUEUE_H

#include <stdint.h>

#include "esp_err.h"

#define DATA_LENGTH_MAX 64

typedef struct {
    int64_t timestamp_us;
    uint8_t data[64];
    size_t data_length;
} key_log_event_t;

esp_err_t key_queue_init(void);
void key_queue_send(key_log_event_t *key_event);
bool key_queue_recv(key_log_event_t *key_event);
void key_queue_clear(void);

#endif // !KEY_QUEUE_H
