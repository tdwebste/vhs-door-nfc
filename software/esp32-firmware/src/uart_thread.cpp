#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/time.h>
#include <assert.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include <esp_types.h>

#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "esp_task_wdt.h"
#include "esp_log.h"

#include "driver/uart.h"
#include "rom/uart.h"

#include "utils.h"

#include "uart_thread.h"
#include "main_thread.h"


#define TAG "UART"


TaskHandle_t  UART_taskHandle  = NULL;
QueueHandle_t UART_queueHandle = NULL;

static StaticQueue_t    UART_queueStructure;
static UartNotification UART_queueStorage[8] = {};


#define UART_FLUSH() while (uart_rx_one_char(&ch) == ESP_OK)
#define UART_WAIT_KEY()                     \
    while (uart_rx_one_char(&ch) != ESP_OK) \
    vTaskDelay(1 / portTICK_PERIOD_MS)

#define STM32_UART_TXD (GPIO_NUM_4)
#define STM32_UART_RXD (GPIO_NUM_36)
#define STM32_UART_RTS (UART_PIN_NO_CHANGE)
#define STM32_UART_CTS (UART_PIN_NO_CHANGE)

#define STM32_UART_BUFFER_SIZE 1024
static char stm32UartBuffer[STM32_UART_BUFFER_SIZE] = {};


static void uart_task(void* pvParameters) {
    ESP_LOGI(TAG, "UART task running...");

    const char* UART_cmd_ready          = "ESP32_READY\n";
    const char* UART_cmd_play_beep_01   = "PLAY_BEEP_01\n";
    const char* UART_cmd_play_beep_02   = "PLAY_BEEP_02\n";
    const char* UART_cmd_play_beep_03   = "PLAY_BEEP_03\n";
    const char* UART_cmd_play_beep_04   = "PLAY_BEEP_04\n";
    const char* UART_cmd_play_beep_05   = "PLAY_BEEP_05\n";
    const char* UART_cmd_play_beep_06   = "PLAY_BEEP_06\n";
    const char* UART_cmd_play_buzzer_01 = "PLAY_BUZZER_01\n";
    const char* UART_cmd_play_buzzer_02 = "PLAY_BUZZER_02\n";
    const char* UART_cmd_play_success   = "PLAY_SUCCESS\n";
    const char* UART_cmd_play_failure   = "PLAY_FAILURE\n";
    const char* UART_cmd_play_smb       = "PLAY_SMB\n";

    const char* UART_cmd_lock_door   = "LOCK_DOOR\n";
    const char* UART_cmd_unlock_door = "UNLOCK_DOOR\n";

    const char* rfid_cmd_prefix = "RFID:";
    const char* pin_cmd_prefix  = "PIN:";

    uart_write_bytes(UART_NUM_1, (const char*)UART_cmd_ready, strlen(UART_cmd_ready));

    while (1) {
        // Check for any waiting notifications
        UartNotification notification;
        while (xQueueReceive(UART_queueHandle, &notification, 0) == pdTRUE) {
            const char* message = NULL;
            if (notification == UART_NOTIFICATION_PlayBeepShortMedium) {
                message = UART_cmd_play_beep_01;
            } else if (notification == UART_NOTIFICATION_PlayBeepShortLow) {
                message = UART_cmd_play_beep_02;
            } else if (notification == UART_NOTIFICATION_PlayBeepLongMedium) {
                message = UART_cmd_play_beep_03;
            } else if (notification == UART_NOTIFICATION_PlayBeepLongLow) {
                message = UART_cmd_play_beep_04;
            } else if (notification == UART_NOTIFICATION_PlayBeepShortHigh) {
                message = UART_cmd_play_beep_05;
            } else if (notification == UART_NOTIFICATION_PlayBeepLongHigh) {
                message = UART_cmd_play_beep_06;
            } else if (notification == UART_NOTIFICATION_PlayBuzzer01) {
                message = UART_cmd_play_buzzer_01;
            } else if (notification == UART_NOTIFICATION_PlayBuzzer02) {
                message = UART_cmd_play_buzzer_02;
            } else if (notification == UART_NOTIFICATION_PlaySuccess) {
                message = UART_cmd_play_success;
            } else if (notification == UART_NOTIFICATION_PlayFailure) {
                message = UART_cmd_play_failure;
            } else if (notification == UART_NOTIFICATION_PlaySmb) {
                message = UART_cmd_play_smb;
            } else if (notification == UART_NOTIFICATION_LockDoor) {
                message = UART_cmd_lock_door;
            } else if (notification == UART_NOTIFICATION_UnlockDoor) {
                message = UART_cmd_unlock_door;
            }

            if (message != NULL) {
                uart_write_bytes(UART_NUM_1, (const char*)message, strlen(message));
            }
        }

        // Read data from the UART
        int len = uart_read_bytes(UART_NUM_1, (uint8_t*)stm32UartBuffer, STM32_UART_BUFFER_SIZE - 1, 20 / portTICK_RATE_MS);
        if (len > 0) {
            stm32UartBuffer[len] = '\0';
            // char rfidData[512] = {};
            // *rfidData = '\0';
            // for (int i = 0; i < len; i++)
            // {
            //     char data[8];
            //     sprintf(data, "%02x", stm32UartBuffer[i]);
            //     strcat(rfidData, data);
            //     if (i != (len - 1))
            //     {
            //         strcat(rfidData, ":");
            //     }
            // }
            // ESP_LOGI("RFID", "%d bytes received: %s", len, rfidData);
            // ESP_LOGI("UART", "%d bytes received", len);
            // printf(stm32UartBuffer);
            // printf("\n");
            if (strstr(stm32UartBuffer, rfid_cmd_prefix) == stm32UartBuffer) {
                for (int i = 0; i < len; i++) {
                    if (stm32UartBuffer[i] == '\n') {
                        stm32UartBuffer[i] = '\0';
                    }
                }

                len            = strlen(stm32UartBuffer) - strlen(rfid_cmd_prefix);
                const char* id = stm32UartBuffer + strlen(rfid_cmd_prefix);

                MainNotificationArgs mainNotificationArgs;
                bzero(&mainNotificationArgs, sizeof(MainNotificationArgs));
                mainNotificationArgs.notification  = MAIN_NOTIFICATION_RfidReady;
                mainNotificationArgs.rfid.idLength = len;
                memcpy(mainNotificationArgs.rfid.id, id, len);
                if (xQueueSendToBack(MAIN_queueHandle, &mainNotificationArgs, 100 / portTICK_PERIOD_MS) != pdTRUE) {
                    // Erk. Did not add to the queue. Oh well? User can just try again when they realize...
                }
            } else if (strstr(stm32UartBuffer, pin_cmd_prefix) == stm32UartBuffer) {
                for (int i = 0; i < len; i++) {
                    if (stm32UartBuffer[i] == '\n') {
                        stm32UartBuffer[i] = '\0';
                    }
                }

                len             = strlen(stm32UartBuffer) - strlen(pin_cmd_prefix);
                const char* pin = stm32UartBuffer + strlen(pin_cmd_prefix);

                MainNotificationArgs mainNotificationArgs;
                bzero(&mainNotificationArgs, sizeof(MainNotificationArgs));
                mainNotificationArgs.notification = MAIN_NOTIFICATION_PinReady;
                mainNotificationArgs.pin.code     = atol(pin);
                if (xQueueSendToBack(MAIN_queueHandle, &mainNotificationArgs, 100 / portTICK_PERIOD_MS) != pdTRUE) {
                    // Erk. Did not add to the queue. Oh well? User can just try again when they realize...
                }
            }
        }
        // Write data back to the UART
        // uart_write_bytes(UART_NUM_1, (const char *) data, len);

        // taskYIELD();
    }
}

//
void uart_init() {
    ESP_LOGI(TAG, "Initializing UART...");

    UART_queueHandle = xQueueCreateStatic(ARRAY_COUNT(UART_queueStorage), sizeof(UartNotification), (uint8_t*)UART_queueStorage, &UART_queueStructure);

    /* Configure parameters of an UART driver,
     * communication pins and install the driver */
    uart_config_t uart_config = {
        .baud_rate           = 115200,
        .data_bits           = UART_DATA_8_BITS,
        .parity              = UART_PARITY_DISABLE,
        .stop_bits           = UART_STOP_BITS_1,
        .flow_ctrl           = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
        .use_ref_tick        = false
    };
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_1, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_1, STM32_UART_TXD, STM32_UART_RXD, STM32_UART_RTS, STM32_UART_CTS));
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_1, STM32_UART_BUFFER_SIZE * 2, 0, 0, NULL, 0));
}

//
void uart_thread_create() {
    xTaskCreate(&uart_task, "uart_task", 4 * 1024, NULL, 10, &UART_taskHandle);
}
