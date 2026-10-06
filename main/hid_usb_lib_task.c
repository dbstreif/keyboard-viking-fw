#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "esp_err.h"
#include "esp_log.h"
#include "usb/usb_host.h"
#include "errno.h"
#include "driver/gpio.h"
#include "esp_timer.h"

#include "usb/hid_host.h"
#include "usb/hid_usage_keyboard.h"
#include "keyboard_info.h"
#include "key_queue.h"

static const char *TAG = "usb_lib_task";

/**
 * @brief HID Protocol string names
 */
static const char *hid_proto_name_str[] = {
    "NONE",
    "KEYBOARD",
};


/**
 * @brief USB HID Host interface callback
 *
 * @param[in] hid_device_handle  HID Device handle
 * @param[in] event              HID Host interface event
 * @param[in] arg                Pointer to arguments, does not used
 */
void hid_host_interface_callback(hid_host_device_handle_t hid_device_handle,
                                 const hid_host_interface_event_t event,
                                 void *arg)
{
    uint8_t data[DATA_LENGTH_MAX] = { 0 };
    size_t data_length = 0;
    hid_host_dev_params_t dev_params;
    ESP_ERROR_CHECK(hid_host_device_get_params(hid_device_handle, &dev_params));

    switch (event) {
    case HID_HOST_INTERFACE_EVENT_INPUT_REPORT:
        ESP_ERROR_CHECK(hid_host_device_get_raw_input_report_data(hid_device_handle,
                                                                  data,
                                                                  64,
                                                                  &data_length));



        // TODO: Fill out ch9329 forwarding
        // Here is where we should queue raw report data for logging
        // Here is where we should make API calls for ch9329 forwarding
        if (HID_SUBCLASS_BOOT_INTERFACE == dev_params.sub_class) {
            if (HID_PROTOCOL_KEYBOARD == dev_params.proto) {
                key_log_event_t key_event = {
                    .timestamp_us = esp_timer_get_time(),
                    .data_length = data_length
                };

                memcpy(key_event.data, data, sizeof(uint8_t) * DATA_LENGTH_MAX);

                key_queue_send(&key_event);
            }
        }

        break;
    case HID_HOST_INTERFACE_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "HID Device, protocol '%s' DISCONNECTED",
                 hid_proto_name_str[dev_params.proto]);
        ESP_ERROR_CHECK(hid_host_device_close(hid_device_handle));
        keyboard_info_clear();
        key_queue_clear();
        break;
    case HID_HOST_INTERFACE_EVENT_TRANSFER_ERROR:
        ESP_LOGI(TAG, "HID Device, protocol '%s' TRANSFER_ERROR",
                 hid_proto_name_str[dev_params.proto]);
        keyboard_info_clear();
        key_queue_clear();
        break;
    default:
        ESP_LOGW(TAG, "HID Device, protocol '%s' Unhandled event: %d (possibly suspend/resume)",
                 hid_proto_name_str[dev_params.proto],
                 event);
        keyboard_info_clear();
        key_queue_clear();
        break;
    }
}

/**
 * @brief USB HID Host Device event
 *
 * @param[in] hid_device_handle  HID Device handle
 * @param[in] event              HID Host Device event
 * @param[in] arg                Pointer to arguments, does not used
 */
void hid_host_device_event(hid_host_device_handle_t hid_device_handle,
                           const hid_host_driver_event_t event,
                           void *arg)
{
    hid_host_dev_params_t dev_params;
    ESP_ERROR_CHECK(hid_host_device_get_params(hid_device_handle, &dev_params));
    
    hid_host_dev_info_t dev_info;
    ESP_ERROR_CHECK(hid_host_get_device_info(hid_device_handle, &dev_info));

    switch (event) {
    case HID_HOST_DRIVER_EVENT_CONNECTED:
        ESP_LOGI(TAG, "HID Device, protocol '%s' CONNECTED",
                 hid_proto_name_str[dev_params.proto]);

        keyboard_info_t keyboard_info = {
            .vid = dev_info.VID,
            .pid = dev_info.PID,
            .address = dev_params.addr,
            .interface_number = dev_params.iface_num,
            .subclass = dev_params.sub_class,
            .protocol = dev_params.proto,
        };

        memcpy(keyboard_info.manufacturer, dev_info.iManufacturer, sizeof(wchar_t[32]));
        memcpy(keyboard_info.product, dev_info.iProduct, sizeof(wchar_t[32]));
        memcpy(keyboard_info.serial, dev_info.iSerialNumber, sizeof(wchar_t[32]));

        keyboard_info_store(&keyboard_info);

        const hid_host_device_config_t dev_config = {
            .callback = hid_host_interface_callback,
            .callback_arg = NULL
        };

        if (dev_params.proto != HID_PROTOCOL_NONE) {
            ESP_ERROR_CHECK(hid_host_device_open(hid_device_handle, &dev_config));
            if (HID_SUBCLASS_BOOT_INTERFACE == dev_params.sub_class) {
                ESP_ERROR_CHECK(hid_class_request_set_protocol(hid_device_handle, HID_REPORT_PROTOCOL_BOOT));
                if (HID_PROTOCOL_KEYBOARD == dev_params.proto) {
                    ESP_ERROR_CHECK(hid_class_request_set_idle(hid_device_handle, 0, 0));
                }
            }
            ESP_ERROR_CHECK(hid_host_device_start(hid_device_handle));
        }
        break;
    default:
        break;
    }
}

/**
 * @brief Start USB Host install and handle common USB host library events while app pin not low
 *
 * @param[in] arg  Not used
 */
void usb_lib_task(void *arg)
{
    const usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LOWMED,
    };

    ESP_ERROR_CHECK(usb_host_install(&host_config));
    xTaskNotifyGive(arg);

    while (true) {
        uint32_t event_flags;
        usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
        // In this example, there is only one client registered
        // So, once we deregister the client, this call must succeed with ESP_OK
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            ESP_ERROR_CHECK(usb_host_device_free_all());
            break;
        }
    }

    ESP_LOGI(TAG, "USB shutdown");
    // Clean up USB Host
    vTaskDelay(10); // Short delay to allow clients clean-up
    ESP_ERROR_CHECK(usb_host_uninstall());
    vTaskDelete(NULL);
}
