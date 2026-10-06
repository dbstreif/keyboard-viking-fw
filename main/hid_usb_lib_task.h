#ifndef HID_USB_LIB_TASK_H
#define HID_USB_LIB_TASK_H

#include "usb/hid_host.h"

void usb_lib_task(void *argument);
void hid_host_device_event(hid_host_device_handle_t hid_device_handle,
                           const hid_host_driver_event_t event,
                           void *arg);

#endif // !HID_USB_LIB_TASK_H
