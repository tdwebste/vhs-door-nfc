#ifndef __IS_VHS_OPEN_HTTP_THREAD__H__
#define __IS_VHS_OPEN_HTTP_THREAD__H__

enum IsVHSOpenHttpNotification {
    IS_VHS_OPEN_HTTP_NOTIFICATION_None,
    IS_VHS_OPEN_HTTP_NOTIFICATION_Status,

    IS_VHS_OPEN_HTTP_NOTIFICATION_COUNT
};

//
extern TaskHandle_t IsVHSOpenHttpTaskHandle;

//
void is_vhs_open_http_thread_create();

#endif //__IS_VHS_OPEN_HTTP_THREAD__H__
