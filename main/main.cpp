#include <message.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "driver/gpio.h"
#include <iostream>
#include <string>
#include <stdio.h>
#include "esp_wifi.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"
#include "esp_system.h"
#include "nvs_flash.h"

static const char *TAG = "serverrr";

constexpr gpio_num_t BUTTON_PINS[] = {GPIO_NUM_19, GPIO_NUM_21, GPIO_NUM_22, GPIO_NUM_23};
constexpr gpio_num_t LED_PINS[] = {GPIO_NUM_26, GPIO_NUM_27, GPIO_NUM_14, GPIO_NUM_13};
constexpr int NUM_BUTTONS = sizeof(BUTTON_PINS) / sizeof(BUTTON_PINS[0]);

int clientSocket = -1; 

void initGPIO() {
    for (int i = 0; i < NUM_BUTTONS; ++i) {
        esp_rom_gpio_pad_select_gpio(BUTTON_PINS[i]);
        gpio_set_direction(BUTTON_PINS[i], GPIO_MODE_INPUT);
        gpio_set_pull_mode(BUTTON_PINS[i], GPIO_PULLUP_ONLY);

        esp_rom_gpio_pad_select_gpio(LED_PINS[i]);
        gpio_set_direction(LED_PINS[i], GPIO_MODE_OUTPUT);
        gpio_set_level(LED_PINS[i], 0);
    }
    ESP_LOGI(TAG, "gpio init dokonceny");
}

void sendToClient(int buttonIndex) {
    if (clientSocket >= 0) {
        Message message{BUTTON_PRESSED, buttonIndex};
        std::string messageStr = message.serialize();
        send(clientSocket, messageStr.c_str(), messageStr.length(), 0);
        ESP_LOGI(TAG, "odoslal som klientovi info o stlaceni: btn %d", buttonIndex);
    } else {
        ESP_LOGE(TAG, "ziadny klient neni pripojeny");
    }
}

void clientHandlerTask(void *arg) {
    while (true) {
        char buffer[128];
        int len = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (len <= 0) {
            ESP_LOGI(TAG, "klient odpojeny");
            close(clientSocket);
            clientSocket = -1;
            break;
        }

        buffer[len] = '\0';
        std::string messageStr(buffer);
        Message message = Message::deserialize(messageStr);

        if (message.type == BUTTON_PRESSED && message.buttonIndex >= 0 && message.buttonIndex < NUM_BUTTONS) {
            gpio_set_level(LED_PINS[message.buttonIndex], 1);
            ESP_LOGI(TAG, "pustam led %d na zaklade spravy od klienta ", message.buttonIndex);
        }
    }
    vTaskDelete(nullptr);
}

void buttonMonitorTask(void *arg) {
    int lastButtonState[NUM_BUTTONS] = {1, 1, 1, 1};

    while (true) {
        for (int i = 0; i < NUM_BUTTONS; ++i) {
            int currentState = gpio_get_level(BUTTON_PINS[i]);
            if (currentState == 0 && lastButtonState[i] == 1) { 
                ESP_LOGI(TAG, "btn %d stlaceny", i);
                sendToClient(i); 
            }
            lastButtonState[i] = currentState;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void serverTask(void *arg) {
    int listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (listenSocket < 0) {
        ESP_LOGE(TAG, "faillo creatnut socket: errno %d", errno);
        vTaskDelete(nullptr);
    }

    struct sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(3333);

    if (bind(listenSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        ESP_LOGE(TAG, "faillo bindnut socket: errno %d", errno);
        close(listenSocket);
        vTaskDelete(nullptr);
    }

    if (listen(listenSocket, 1) < 0) {
        ESP_LOGE(TAG, "faillo spustenie listeningu: errno %d", errno);
        close(listenSocket);
        vTaskDelete(nullptr);
    }

    ESP_LOGI(TAG, "pocuvam na porte: %d", 3333);

    while (true) {
        struct sockaddr_in clientAddr{};
        socklen_t clientAddrLen = sizeof(clientAddr);
        clientSocket = accept(listenSocket, (struct sockaddr *)&clientAddr, &clientAddrLen);
        if (clientSocket < 0) {
            ESP_LOGE(TAG, "faillo acceptnutie clienta: errno %d", errno);
            continue;
        }

        ESP_LOGI(TAG, "client connectnuty");
        xTaskCreate(clientHandlerTask, "clientHandlerTask", 4096, nullptr, 10, nullptr);
    }

    close(listenSocket);
    vTaskDelete(nullptr);
}

extern "C" void app_main() {
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_ERROR_CHECK(example_connect());

    initGPIO();
    xTaskCreate(serverTask, "serverTask", 10000, nullptr, 10, nullptr);
    xTaskCreate(buttonMonitorTask, "buttonMonitorTask", 10000, nullptr, 10, nullptr);
}