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

static const char *TAG = "Client";

// GPIO nastavenie
constexpr gpio_num_t BUTTON_PINS[] = {};
constexpr gpio_num_t LED_PINS[] = {};
constexpr int NUM_BUTTONS = sizeof(BUTTON_PINS) / sizeof(BUTTON_PINS[0]);

int serverSocket = -1; 

// Inicializácia GPIO
void initGPIO() {
    for (int i = 0; i < NUM_BUTTONS; ++i) {
        esp_rom_gpio_pad_select_gpio(BUTTON_PINS[i]);
        gpio_set_direction(BUTTON_PINS[i], GPIO_MODE_INPUT);
        gpio_set_pull_mode(BUTTON_PINS[i], GPIO_PULLUP_ONLY);

        esp_rom_gpio_pad_select_gpio(LED_PINS[i]);
        gpio_set_direction(LED_PINS[i], GPIO_MODE_OUTPUT);
        gpio_set_level(LED_PINS[i], 0);
    }
    ESP_LOGI(TAG, "gpio init finishnuty");
}

void sendToServer(int buttonIndex) {
    if (serverSocket >= 0) {
        Message message{BUTTON_PRESSED, buttonIndex};
        std::string messageStr = message.serialize();
        send(serverSocket, messageStr.c_str(), messageStr.length(), 0);
        ESP_LOGI(TAG, "odoslal som spravu serveru: btn %d", buttonIndex);
    } else {
        ESP_LOGE(TAG, "neni pripojenie na server");
    }
}

void serverHandlerTask(void *arg) {
    while (true) {
        char buffer[128];
        int len = recv(serverSocket, buffer, sizeof(buffer) - 1, 0);
        if (len <= 0) {
            ESP_LOGI(TAG, "ukoncene spojenie so serverom");
            close(serverSocket);
            serverSocket = -1;
            break;
        }

        buffer[len] = '\0';
        std::string messageStr(buffer);
        Message message = Message::deserialize(messageStr);

        if (message.type == BUTTON_PRESSED && message.buttonIndex >= 0 && message.buttonIndex < NUM_BUTTONS) {
            gpio_set_level(LED_PINS[message.buttonIndex], 1);
            ESP_LOGI(TAG, "spustil som led %d na zaklade message zo servera", message.buttonIndex);
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
                ESP_LOGI(TAG, "Tlačidlo %d stlačené", i);
                sendToServer(i); 
            }
            lastButtonState[i] = currentState;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void clientTask(void *arg) {
    struct sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr("192.168.1.100");
    serverAddr.sin_port = htons(3333);

    while (true) {
        serverSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (serverSocket < 0) {
            ESP_LOGE(TAG, "faillo vytvorenie socketu: errno %d", errno);
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        ESP_LOGI(TAG, "pripajam na server...");
        if (connect(serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
            ESP_LOGE(TAG, "faillo pripojenie na server: errno %d", errno);
            close(serverSocket);
            serverSocket = -1;
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        ESP_LOGI(TAG, "successfully som sa pripojil na server");
        xTaskCreate(serverHandlerTask, "serverHandlerTask", 4096, nullptr, 10, nullptr);
        break;
    }

    vTaskDelete(nullptr);
}

extern "C" void app_main() {
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_ERROR_CHECK(example_connect());
    
    initGPIO();

    xTaskCreate(clientTask, "clientTask", 10000, nullptr, 10, nullptr);
    xTaskCreate(buttonMonitorTask, "buttonMonitorTask", 10000, nullptr, 10, nullptr);
}