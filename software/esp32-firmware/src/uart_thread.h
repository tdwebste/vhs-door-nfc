#ifndef __UART_THREAD__H__
#define __UART_THREAD__H__

enum UartNotification {
    UART_NOTIFICATION_None,

    UART_NOTIFICATION_PlayBeepShortMedium,
    UART_NOTIFICATION_PlayBeepShortLow,
    UART_NOTIFICATION_PlayBeepLongMedium,
    UART_NOTIFICATION_PlayBeepLongLow,
    UART_NOTIFICATION_PlayBeepShortHigh,
    UART_NOTIFICATION_PlayBeepLongHigh,
    UART_NOTIFICATION_PlayBuzzer01,
    UART_NOTIFICATION_PlayBuzzer02,
    UART_NOTIFICATION_PlaySuccess,
    UART_NOTIFICATION_PlayFailure,
    UART_NOTIFICATION_PlaySmb,

    UART_NOTIFICATION_LockDoor,
    UART_NOTIFICATION_UnlockDoor,

    UART_NOTIFICATION_COUNT
};


extern TaskHandle_t  UART_taskHandle;
extern QueueHandle_t UART_queueHandle;


//
void uart_init();
void uart_thread_create();

#endif //__UART_THREAD__H__
