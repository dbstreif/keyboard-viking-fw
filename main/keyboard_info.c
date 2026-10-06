#include <assert.h>
#include <string.h>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "keyboard_info.h"
#include "helpers.h"

#define KEYBOARD_INFO_PACKAGE_SIZE 768
#define KEYBOARD_UTF8_STRING_SIZE (KEYBOARD_INFO_STRING_LENGTH * 4)

static keyboard_info_t current_keyboard;
static SemaphoreHandle_t keyboard_mutex;

esp_err_t keyboard_info_init(void)
{
    keyboard_mutex = xSemaphoreCreateMutex();
    if (keyboard_mutex == NULL) return ESP_ERR_INVALID_STATE;
    return ESP_OK;
}

void keyboard_info_store(const keyboard_info_t *info)
{
    xSemaphoreTake(keyboard_mutex, portMAX_DELAY);

    current_keyboard = *info;
    current_keyboard.connected = true;

    xSemaphoreGive(keyboard_mutex);
}

bool keyboard_info_load(keyboard_info_t *info)
{
    xSemaphoreTake(keyboard_mutex, portMAX_DELAY);

    *info = current_keyboard;
    bool connected = current_keyboard.connected;

    xSemaphoreGive(keyboard_mutex);

    return connected;
}

void keyboard_info_clear(void)
{
    xSemaphoreTake(keyboard_mutex, portMAX_DELAY);

    memset(&current_keyboard, 0, sizeof(current_keyboard));

    xSemaphoreGive(keyboard_mutex);
}

const char *keyboard_info_create_package(void)
{
    static char package[KEYBOARD_INFO_PACKAGE_SIZE];

    keyboard_info_t info;

    char manufacturer[KEYBOARD_UTF8_STRING_SIZE];
    char product[KEYBOARD_UTF8_STRING_SIZE];
    char serial[KEYBOARD_UTF8_STRING_SIZE];

    if (!keyboard_info_load(&info)) {
        snprintf(
            package,
            sizeof(package),
            "Keyboard: disconnected\n"
        );

        return package;
    }

    wchar_to_utf8(
        info.manufacturer,
        manufacturer,
        sizeof(manufacturer)
    );

    wchar_to_utf8(
        info.product,
        product,
        sizeof(product)
    );

    wchar_to_utf8(
        info.serial,
        serial,
        sizeof(serial)
    );

    snprintf(
        package,
        sizeof(package),
        "Keyboard: connected\n"
        "VID: %04X\n"
        "PID: %04X\n"
        "Manufacturer: %s\n"
        "Product: %s\n"
        "Serial: %s\n"
        "USB address: %u\n"
        "Interface: %u\n"
        "Subclass: %u\n"
        "Protocol: %u\n",
        info.vid,
        info.pid,
        manufacturer,
        product,
        serial,
        info.address,
        info.interface_number,
        info.subclass,
        info.protocol
    );

    return package;
}
