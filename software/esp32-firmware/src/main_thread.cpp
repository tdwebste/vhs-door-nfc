#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <inttypes.h>
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

#include "utils.h"

#include "nomos_http_thread.h"
#include "uart_thread.h"
#include "main_thread.h"

#include "main_state_machine.h"

#define TAG "MAIN"

TaskHandle_t  MAIN_taskHandle  = NULL;
QueueHandle_t MAIN_queueHandle = NULL;

static StaticQueue_t        MAIN_queueStructure;
static MainNotificationArgs MAIN_queueStorage[8] = {};

static MainStateMachine mainStateMachine;

//
static void processRfidReadyNotification(const MainNotificationArgs& notificationArgs) {
    if (notificationArgs.rfid.idLength == 7) {
        const uint8_t* id = notificationArgs.rfid.id;
        sprintf(nomosHttpRequestBody, "{ \"rfid\": \"%02X:%02X:%02X:%02X:%02X:%02X:%02X\" }", id[0], id[1], id[2], id[3], id[4], id[5], id[6]);

        xTaskNotify(NomosHttpTaskHandle, NOMOS_HTTP_NOTIFICATION_RequestRfid, eSetValueWithOverwrite);

        mainStateMachine.SetState(MainStateMachine::STATE_ValidatingRFID);

        //
        UartNotification notification = UART_NOTIFICATION_PlayBeepShortHigh;
        if (xQueueSendToBack(UART_queueHandle, &notification, 0) != pdTRUE) {
            // Erk. Did not add to the queue. Oh well? It's just a sfx
        }
    } else {
        ESP_LOGE(TAG, "Unexpected RFID card ID length: %d", (int)notificationArgs.rfid.idLength);

        UartNotification notification = UART_NOTIFICATION_PlayFailure;
        if (xQueueSendToBack(UART_queueHandle, &notification, 0) != pdTRUE) {
            // Erk. Did not add to the queue. Oh well? It's just a sfx
        }
    }
}

//
static void processPinReadyNotification(const MainNotificationArgs& notificationArgs) {
    if (mainStateMachine.GetState() != MainStateMachine::STATE_WaitingForPIN) {
        ESP_LOGE(TAG, "Unexpected state when PIN entered: %d", (int)mainStateMachine.GetState());

        UartNotification notification = UART_NOTIFICATION_PlayFailure;
        if (xQueueSendToBack(UART_queueHandle, &notification, 0) != pdTRUE) {
            // Erk. Did not add to the queue. Oh well? It's just a sfx
        }

        return;
    }

    sprintf(nomosHttpRequestBody, "{ \"pin\": \"%08d\" }", notificationArgs.pin.code);

    xTaskNotify(NomosHttpTaskHandle, NOMOS_HTTP_NOTIFICATION_RequestPin, eSetValueWithOverwrite);

    mainStateMachine.SetState(MainStateMachine::STATE_ValidatingPIN);

    //
    UartNotification notification = UART_NOTIFICATION_PlayBeepShortHigh;
    if (xQueueSendToBack(UART_queueHandle, &notification, 0) != pdTRUE) {
        // Erk. Did not add to the queue. Oh well? It's just a sfx
    }
}

//
static void processNomosHttpRequestResultReadyNotification(const MainNotificationArgs& notificationArgs) {
    if (notificationArgs.NomosHttpRequestResult.httpNotification == NOMOS_HTTP_NOTIFICATION_RequestValidate) {
        // Unused at this time
        if (notificationArgs.NomosHttpRequestResult.success) {
        }
    } else if (notificationArgs.NomosHttpRequestResult.httpNotification == NOMOS_HTTP_NOTIFICATION_RequestRfid) {
        if (notificationArgs.NomosHttpRequestResult.success) {
            const NomosHttpResponseResult& result = notificationArgs.NomosHttpRequestResult.result;
            if ((result.userId > 0) && result.bValidUser) {
                if (result.bHasDoorAccess && result.bHasBeenVetted) {
                    // Magical RFID card. Such power. Much access. So fast. Wow.
                    ESP_LOGI(TAG, "RFID-only access granted.");

                    mainStateMachine.SetState(MainStateMachine::STATE_AccessGranted);

                    UartNotification notification = UART_NOTIFICATION_PlaySmb;
                    if (xQueueSendToBack(UART_queueHandle, &notification, 0) != pdTRUE) {
                        // Erk. Did not add to the queue. Oh well? It's just a sfx
                    }
                    notification = UART_NOTIFICATION_PlayBuzzer01;
                    if (xQueueSendToBack(UART_queueHandle, &notification, 10 / portTICK_PERIOD_MS) != pdTRUE) {
                        // Erk. Did not add to the queue. Oh well? It's just a sfx
                    }
                } else {
                    // Check if VHS is open as that'll dictate if we just open the door or require further PIN authentication
                    ESP_LOGI(TAG, "RFID validated, checking if VHS is open.");

                    xTaskNotify(IsVHSOpenHttpTaskHandle, IS_VHS_OPEN_HTTP_NOTIFICATION_Status, eSetValueWithOverwrite);

                    mainStateMachine.SetState(MainStateMachine::STATE_IsVHSOpen);

                    // Don't play the SFX here, as we're not ready for the PIN until we've checked if VHS is currently open or not.
                    // UartNotification notification = UART_NOTIFICATION_PlaySuccess;
                    // if (xQueueSendToBack(UART_queueHandle, &notification, 0) != pdTRUE) {
                    //     // Erk. Did not add to the queue. Oh well? It's just a sfx
                    // }
                }
            } else {
                // No such user
                ESP_LOGI(TAG, "Invalid RFID card.");

                mainStateMachine.SetState(MainStateMachine::STATE_Idle);

                UartNotification notification = UART_NOTIFICATION_PlayFailure;
                if (xQueueSendToBack(UART_queueHandle, &notification, 10 / portTICK_PERIOD_MS) != pdTRUE) {
                    // Erk. Did not add to the queue. Oh well? It's just a sfx
                }
            }
        } else {
            // Request failed, likely due to missing fields in the results because the user doesn't have access or is invalid
            ESP_LOGI(TAG, "RFID request failed.");

            mainStateMachine.SetState(MainStateMachine::STATE_Idle);

            UartNotification notification = UART_NOTIFICATION_PlayFailure;
            if (xQueueSendToBack(UART_queueHandle, &notification, 10 / portTICK_PERIOD_MS) != pdTRUE) {
                // Erk. Did not add to the queue. Oh well? It's just a sfx
            }
        }
    } else if (notificationArgs.NomosHttpRequestResult.httpNotification == NOMOS_HTTP_NOTIFICATION_RequestPin) {
        if (notificationArgs.NomosHttpRequestResult.success) {
            const NomosHttpResponseResult& result = notificationArgs.NomosHttpRequestResult.result;
            if ((result.userId > 0) && result.bValidUser && result.bHasDoorAccess && result.bHasBeenVetted) {
                // Success! Let the user enter.
                ESP_LOGI(TAG, "PIN valid. Access granted.");

                mainStateMachine.SetState(MainStateMachine::STATE_AccessGranted);

                UartNotification notification = UART_NOTIFICATION_PlaySuccess;
                if (xQueueSendToBack(UART_queueHandle, &notification, 0) != pdTRUE) {
                    // Erk. Did not add to the queue. Oh well? It's just a sfx
                }
                notification = UART_NOTIFICATION_PlayBuzzer01;
                if (xQueueSendToBack(UART_queueHandle, &notification, 10 / portTICK_PERIOD_MS) != pdTRUE) {
                    // Erk. Did not add to the queue. Oh well? It's just a sfx
                }
            } else {
                // No access
                ESP_LOGI(TAG, "PIN not valid or user doesn't have access.");

                mainStateMachine.SetState(MainStateMachine::STATE_WaitingForPIN);

                UartNotification notification = UART_NOTIFICATION_PlayFailure;
                if (xQueueSendToBack(UART_queueHandle, &notification, 10 / portTICK_PERIOD_MS) != pdTRUE) {
                    // Erk. Did not add to the queue. Oh well? It's just a sfx
                }
            }
        } else {
            // Request failed, likely due to missing fields in the results because the user doesn't have access or is invalid
            ESP_LOGI(TAG, "PIN request failed.");

            mainStateMachine.SetState(MainStateMachine::STATE_WaitingForPIN);

            UartNotification notification = UART_NOTIFICATION_PlayFailure;
            if (xQueueSendToBack(UART_queueHandle, &notification, 10 / portTICK_PERIOD_MS) != pdTRUE) {
                // Erk. Did not add to the queue. Oh well? It's just a sfx
            }
        }
    } else {
        // Error
        ESP_LOGE(TAG, "Unknown Nomos httpNotification: %d", (int)notificationArgs.NomosHttpRequestResult.httpNotification);

        mainStateMachine.SetState(MainStateMachine::STATE_Idle);
    }
}

//
static void processIsVHSOpenHttpRequestResultReadyNotification(const MainNotificationArgs& notificationArgs) {
    if (notificationArgs.IsVHSOpenHttpRequestResult.httpNotification == IS_VHS_OPEN_HTTP_NOTIFICATION_Status) {
        if (notificationArgs.IsVHSOpenHttpRequestResult.success &&
            notificationArgs.IsVHSOpenHttpRequestResult.open) {
            // VHS is open - let the member in
            ESP_LOGI(TAG, "VHS is open. Access granted.");

            mainStateMachine.SetState(MainStateMachine::STATE_AccessGranted);

            UartNotification notification = UART_NOTIFICATION_PlaySmb;
            if (xQueueSendToBack(UART_queueHandle, &notification, 0) != pdTRUE) {
                // Erk. Did not add to the queue. Oh well? It's just a sfx
            }
            notification = UART_NOTIFICATION_PlayBuzzer01;
            if (xQueueSendToBack(UART_queueHandle, &notification, 10 / portTICK_PERIOD_MS) != pdTRUE) {
                // Erk. Did not add to the queue. Oh well? It's just a sfx
            }
        } else {
            // VHS is closed - require a keyholder to enter their pin to open the door
            ESP_LOGI(TAG, "VHS is closed, PIN required.");

            mainStateMachine.SetState(MainStateMachine::STATE_WaitingForPIN);

            //
            UartNotification notification = UART_NOTIFICATION_PlaySuccess;
            if (xQueueSendToBack(UART_queueHandle, &notification, 0) != pdTRUE) {
                // Erk. Did not add to the queue. Oh well? It's just a sfx
            }
        }
    } else {
        // Error
        ESP_LOGE(TAG, "Unknown IsVHSOpen httpNotification: %d", (int)notificationArgs.IsVHSOpenHttpRequestResult.httpNotification);

        mainStateMachine.SetState(MainStateMachine::STATE_Idle);
    }
}


//
static void onStateChange(MainStateMachine::State_e oldState, MainStateMachine::State_e newState) {
    time_t    now      = 0;
    struct tm timeinfo = {};
    //
    time(&now);
    localtime_r(&now, &timeinfo);
    //
    char dateTimeStr[64];
    strftime(dateTimeStr, sizeof(dateTimeStr), "%c", &timeinfo);

    ESP_LOGI(TAG, "State change: %s -> %s at %s",
             MainStateMachine::GetStateName(oldState),
             MainStateMachine::GetStateName(newState),
             dateTimeStr);

    if (newState == MainStateMachine::STATE_AccessGranted) {
        // Energize the electronic strike to open the door
        UartNotification notification = UART_NOTIFICATION_UnlockDoor;
        if (xQueueSendToBack(UART_queueHandle, &notification, 0) != pdTRUE) {
            // Erk. Did not add to the queue. This one is a problem - we failed to open the door!
        }
    } else {
        // De-energize the electronic strike to ensure the door is locked
        UartNotification notification = UART_NOTIFICATION_LockDoor;
        if (xQueueSendToBack(UART_queueHandle, &notification, 0) != pdTRUE) {
            // Erk. Did not add to the queue. This one is a BIG problem - we failed to lock the door!!!
        }
    }
}

//
void main_thread_init() {
    mainStateMachine.init(&onStateChange);

    MAIN_taskHandle = xTaskGetCurrentTaskHandle();

    MAIN_queueHandle = xQueueCreateStatic(ARRAY_COUNT(MAIN_queueStorage), sizeof(MainNotificationArgs), (uint8_t*)MAIN_queueStorage, &MAIN_queueStructure);
}

void main_thread_run() {
    while (1) {
        mainStateMachine.Update();

        //
        MainNotificationArgs notificationArgs;
        if (xQueueReceive(MAIN_queueHandle, &notificationArgs, 1000 / portTICK_PERIOD_MS) == pdTRUE) {
            if (notificationArgs.notification == MAIN_NOTIFICATION_RfidReady) {
                processRfidReadyNotification(notificationArgs);
            } else if (notificationArgs.notification == MAIN_NOTIFICATION_PinReady) {
                processPinReadyNotification(notificationArgs);
            } else if (notificationArgs.notification == MAIN_NOTIFICATION_NomosHttpRequestResultReady) {
                processNomosHttpRequestResultReadyNotification(notificationArgs);
            } else if (notificationArgs.notification == MAIN_NOTIFICATION_IsVHSOpenHttpRequestResultReady) {
                processIsVHSOpenHttpRequestResultReadyNotification(notificationArgs);
            } else {
                ESP_LOGE(TAG, "Unknown MainNotification: %d", (int)notificationArgs.notification);
            }
        }
    }
}
