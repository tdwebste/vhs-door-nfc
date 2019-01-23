#ifndef __MAIN_THREAD__H__
#define __MAIN_THREAD__H__

#include "nomos_http_thread.h"
#include "is_vhs_open_http_thread.h"


enum MainNotification {
    MAIN_NOTIFICATION_None,
    MAIN_NOTIFICATION_RfidReady,
    MAIN_NOTIFICATION_PinReady,
    MAIN_NOTIFICATION_NomosHttpRequestResultReady,
    MAIN_NOTIFICATION_IsVHSOpenHttpRequestResultReady,

    MAIN_NOTIFICATION_COUNT
};

struct MainNotificationArgs {
    MainNotification notification;

    union {
        struct {
            uint8_t id[7];
            uint8_t idLength;
        } rfid;
        struct {
            uint32_t code;
        } pin;
        struct {
            NomosHttpResponseResult result;

            NomosHttpNotification httpNotification;
            bool                  success;
        } NomosHttpRequestResult;
        struct {
            IsVHSOpenHttpNotification httpNotification;
            bool                      open;
            bool                      success;
        } IsVHSOpenHttpRequestResult;
    };
};

extern TaskHandle_t  MAIN_taskHandle;
extern QueueHandle_t MAIN_queueHandle;

//
void main_thread_init();
void main_thread_run();

#endif //__MAIN_THREAD__H__
