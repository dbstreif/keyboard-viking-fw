#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"
#include "esp_check.h"
#include "esp_console.h"

#include "keyboard_info.h"
#include "helpers.h"

#define SERVER_PORT 3232
#define COMMAND_COUNT 2

static const char *TAG = "command_server";

typedef struct {
    int client_sock;
    esp_console_cmd_t commands[COMMAND_COUNT + 1];
} command_context_t;

//Forward Declares
static int send_all(int socket_fd, const char *data, size_t length);

static int help_callback(void *context, int argc, char **argv) {
    if (context == NULL) {
        ESP_LOGE(TAG, "help_callback: Command context is NULL!");
        return 1;
    }

    command_context_t *command_context = (command_context_t *)context;
    int sock = command_context->client_sock;
    esp_console_cmd_t *commands = command_context->commands;

    if (argc > 1) {
        const char errstr[] = "Invalid arguments\n";
        ESP_LOGE(TAG, "Called help with invalid args!");
        if (send_all(sock, errstr, sizeof(errstr)) < 0) return 1;
        return 0;
    }

    if (send_all(sock, "Available Commands:\n", 20) < 0) return 1;

    for (int i = 0; commands[i].command != NULL; i++) {
        if (send_all(sock, "\t", 1) < 0) return 1;
        if (send_all(sock, commands[i].command, strlen(commands[i].command)) < 0) return 1;
        if (send_all(sock, "\t\t", 2) < 0) return 1;
        if (send_all(sock, commands[i].help, strlen(commands[i].help)) < 0) return 1;
        if (send_all(sock, "\n", 1) < 0) return 1;
    }

    return 0;
}

static int keyboard_info_callback(void *context, int argc, char **argv) {
    if (context == NULL) {
        ESP_LOGE(TAG, "keyboard_info_callback: Command context is NULL!");
        return 1;
    }

    command_context_t *command_context = context;
    int sock = command_context->client_sock;

    if (argc > 1) {
        const char errstr[] = "Invalid arguments\n";
        ESP_LOGE(TAG, "Called keyboard_info_callback with invalid args!");
        if (send_all(sock, errstr, sizeof(errstr)) < 0) return 1;
        return 0;
    }

    ESP_LOGI(TAG, "Called keyboard_info_callback with sock: %d, command: %s", sock, argv[0]);

    const char *keyboard_info_str = keyboard_info_create_package();
    if (send_all(sock, keyboard_info_str, strlen(keyboard_info_str)) < 0) return 1;

    return 0;
}

static esp_err_t register_command_server_commands(command_context_t *command_context) {
    /*
     * Example Command
     * const esp_console_cmd_t remap_cmd = {
        .command = "remap",
        .help = "Remap one keyboard key to another",
        .hint = "<source> <destination>",
        .func = remap_command,
    };
    * Then call esp_console_cmd_register()
    */

    const esp_console_cmd_t keyboard_info_cmd = {
        .command = "keyboard-info",
        .help = "Display information about the connected keyboard",
        .func_w_context = keyboard_info_callback,
        .context = (void *)command_context
    };

    // help command context with NULL sentinel
    const esp_console_cmd_t commands[COMMAND_COUNT + 1] = {
        {
            .command = "help         ",
            .help = "Display this help menu",
            .func_w_context = NULL,
            .context = NULL
        },
        keyboard_info_cmd,
        {
            .command = NULL,
            .help = NULL,
            .func_w_context = NULL,
            .context = NULL
        }
    };

    memcpy(command_context->commands, commands, sizeof(commands));

    const esp_console_cmd_t help_cmd = {
        .command = "help",
        .help = "Displays available commands",
        .func_w_context = help_callback,
        .context = (void *)command_context
    };

    ESP_RETURN_ON_ERROR(esp_console_cmd_register(&keyboard_info_cmd), TAG,
                        "Failed to register keyboard-info command");

    ESP_RETURN_ON_ERROR(esp_console_cmd_register(&help_cmd), TAG,
                        "Failed to register keyboard-info command");


    ESP_LOGI(TAG, "Registered command server commands");

    return ESP_OK;
}

static esp_err_t configure_console(void) {
    const esp_console_config_t console_config =  {
        .max_cmdline_length = 512,                                        
        .max_cmdline_args = 32,
        .heap_alloc_caps = MALLOC_CAP_DEFAULT,
        .hint_color = 39,
        .hint_bold = 0
    };

    ESP_RETURN_ON_ERROR(esp_console_init(&console_config), TAG, "Failed to init esp_console");

    return ESP_OK;
}

static int send_all(int socket_fd, const char *data, size_t length)
{
    size_t sent = 0;

    while (sent < length) {
        int result = send(socket_fd, data + sent, length - sent, 0);

        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }

            return -1;
        }

        sent += result;
    }

    return 0;
}

static void handle_client(int client_fd)
{
    char buffer[256];

    static const char welcome[] =
        "Keybaord-Viking command server\n"
        "Type something and press Enter.\n"
        "keyboard-viking> ";

    if (send_all(client_fd, welcome, strlen(welcome)) < 0) {
        return;
    }

    while (true) {
        int received = recv(
            client_fd,
            buffer,
            sizeof(buffer) - 1,
            0
        );

        if (received == 0) {
            ESP_LOGI(TAG, "Client disconnected");
            break;
        }

        if (received < 0) {
            if (errno == EINTR) {
                continue;
            }

            ESP_LOGE(TAG, "recv failed: errno %d", errno);
            break;
        }

        buffer[received] = '\0';

        ESP_LOGI(TAG, "Received: %s", buffer);

        const char *command_buffer = trimwhitespace(buffer);

        ESP_LOGI(TAG, "Whitespace trim completed");

        // Do cmd handling here
        ESP_LOGI(TAG, "Starting esp_console_run parsing");
        int command_ret;
        esp_err_t console_run_err = esp_console_run(command_buffer, &command_ret);
        ESP_LOGI(TAG, "Completed esp_console_run parsing");

        if (console_run_err == ESP_ERR_NOT_FOUND) {
            if (send_all(client_fd, "Invalid Command!\n", 19) < 0) break;
            command_ret = 0;
        } else if (console_run_err == ESP_ERR_INVALID_STATE) {
            ESP_LOGE(TAG, "Console Command Library not initialized!!!");
            command_ret = 0;
        }

        if (command_ret) {
            if(send_all(client_fd, "Fatal command error occurred!\n", 30) < 0) break;
        }

        if (send_all(client_fd, "friendly-usb> ", 14) < 0) {
            break;
        }
    }
}

void tcp_server_task(void *argument)
{
    int listen_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);

    if (listen_fd < 0) {
        ESP_LOGE(TAG, "socket failed: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }

    int reuse_address = 1;
    setsockopt(
        listen_fd,
        SOL_SOCKET,
        SO_REUSEADDR,
        &reuse_address,
        sizeof(reuse_address)
    );

    struct sockaddr_in address = {
        .sin_family = AF_INET,
        .sin_port = htons(SERVER_PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };

    if (bind(
            listen_fd,
            (struct sockaddr *)&address,
            sizeof(address)
        ) < 0) {
        ESP_LOGE(TAG, "bind failed: errno %d", errno);
        close(listen_fd);
        vTaskDelete(NULL);
        return;
    }

    if (listen(listen_fd, 1) < 0) {
        ESP_LOGE(TAG, "listen failed: errno %d", errno);
        close(listen_fd);
        vTaskDelete(NULL);
        return;
    }

    // Init console
    ESP_ERROR_CHECK(configure_console());

    command_context_t command_context = {
        .client_sock = -1,
    };

    ESP_ERROR_CHECK(register_command_server_commands(&command_context));

    ESP_LOGI(TAG, "Listening on TCP port %d", SERVER_PORT);

    while (true) {
        struct sockaddr_in client_address;
        socklen_t client_length = sizeof(client_address);

        int client_fd = accept(
            listen_fd,
            (struct sockaddr *)&client_address,
            &client_length
        );

        if (client_fd < 0) {
            ESP_LOGE(TAG, "accept failed: errno %d", errno);
            continue;
        }

        ESP_LOGI(
            TAG,
            "Client connected from %s",
            inet_ntoa(client_address.sin_addr)
        );

        command_context.client_sock = client_fd;
        ESP_LOGI(TAG, "Set client_sock: %d", client_fd);

        handle_client(client_fd);

        shutdown(client_fd, SHUT_RDWR);
        close(client_fd);
    }
}
