#ifndef __NOMOS_HTTP_THREAD__H__
#define __NOMOS_HTTP_THREAD__H__

enum NomosHttpNotification {
    NOMOS_HTTP_NOTIFICATION_None,
    NOMOS_HTTP_NOTIFICATION_RequestValidate,
    NOMOS_HTTP_NOTIFICATION_RequestRfid,
    NOMOS_HTTP_NOTIFICATION_RequestPin,

    NOMOS_HTTP_NOTIFICATION_COUNT
};

enum NomosHttpResponseType {
    NOMOS_RT_JSON,
    NOMOS_RT_BOOLEAN
};

struct NomosHttpResponseResult {
    // NOMOS_RT_JSON
    uint32_t userId;
    bool     bValidUser;
    bool     bHasBeenVetted;
    bool     bHasDoorAccess;

    // NOMOS_RT_BOOLEAN
    bool bValue;
};

//
extern TaskHandle_t NomosHttpTaskHandle;
extern char         nomosHttpRequestBody[64];

//
void nomos_http_thread_create();

#endif //__NOMOS_HTTP_THREAD__H__
