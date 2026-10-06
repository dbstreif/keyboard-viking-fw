#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#define KEYBOARD_INFO_STRING_LENGTH 33

typedef struct {
    bool connected;

    uint16_t vid;
    uint16_t pid;

    uint8_t address;
    uint8_t interface_number;
    uint8_t subclass;
    uint8_t protocol;

    wchar_t manufacturer[KEYBOARD_INFO_STRING_LENGTH];
    wchar_t product[KEYBOARD_INFO_STRING_LENGTH];
    wchar_t serial[KEYBOARD_INFO_STRING_LENGTH];
} keyboard_info_t;

esp_err_t keyboard_info_init(void);
void keyboard_info_store(const keyboard_info_t *info);
bool keyboard_info_load(keyboard_info_t *info);
void keyboard_info_clear(void);
const char *keyboard_info_create_package(void);
