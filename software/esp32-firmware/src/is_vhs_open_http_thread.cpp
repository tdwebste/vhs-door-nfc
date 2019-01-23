#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/time.h>
#include <assert.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <esp_types.h>

#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "esp_task_wdt.h"
#include "esp_log.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

#include "esp_tls.h"

#define ARDUINOJSON_EMBEDDED_MODE 1
#include <ArduinoJson.h>

#include "utils.h"

#include "is_vhs_open_http_thread.h"
#include "main_thread.h"


#define TAG "IS_VHS_OPEN"


TaskHandle_t IsVHSOpenHttpTaskHandle = NULL;


// > openssl s_client -showcerts -connect isvhsopen.com:443 </dev/null
// The CA root cert is the last cert given in the chain of certs.
//
// Cert is embedded in the binary via platformio.ini
extern const uint8_t is_vhs_open_root_cert_pem_start[] asm("_binary_src_is_vhs_open_root_cert_pem_start");
extern const uint8_t is_vhs_open_root_cert_pem_end[] asm("_binary_src_is_vhs_open_root_cert_pem_end");


#define WEB_SERVER "isvhsopen.com"
#define WEB_PORT "443"
#define WEB_URL_STATUS "https://isvhsopen.com/api/status/"

static const char* REQUEST_STATUS = "GET " WEB_URL_STATUS " HTTP/1.0\r\n"
                                    "Host: " WEB_SERVER "\r\n"
                                    "User-Agent: esp-idf/1.0 esp32\r\n"
                                    "Content-Type: text/json\r\n"
                                    "Content-Length: %d\r\n"
                                    "\r\n"
                                    "%s";


static char                       requestBuff[512];
static char                       readBuffer[1 * 1024];
static StaticJsonBuffer<2 * 1024> jsonBuffer; // NOTE: It was observed in NomosHttpThread that the jsonBuffer had to be larger than the readBuffer, otherwise it would sometimes fail to parse

static bool send_request(struct esp_tls* tls, const char* header, const char* body) {
    sprintf(requestBuff, header, (body == NULL) ? 0 : strlen(body), (body == NULL) ? "" : body);
    assert(strlen(requestBuff) < ARRAY_COUNT(requestBuff));

    size_t written_bytes = 0;
    do {
        int ret = esp_tls_conn_write(tls,
                                     requestBuff + written_bytes,
                                     strlen(requestBuff) - written_bytes);
        if (ret > 0) {
            // ESP_LOGI(TAG, "%d bytes written", ret);
            written_bytes += ret;
        } else if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
            ESP_LOGE(TAG, "esp_tls_conn_write  returned 0x%x", ret);
            return false;
        }
    } while (written_bytes < strlen(requestBuff));

    return true;
}

static bool read_response(struct esp_tls* tls, bool* pResult) {
    bzero(readBuffer, ARRAY_COUNT(readBuffer));

    int ret;
    int len = 0;
    // int chunks = 0;

    do {
        int maxLen = (ARRAY_COUNT(readBuffer) - len) - 1;
        ret        = esp_tls_conn_read(tls, (char*)readBuffer + len, maxLen);

        if (ret == MBEDTLS_ERR_SSL_WANT_WRITE || ret == MBEDTLS_ERR_SSL_WANT_READ)
            continue;

        if (ret < 0) {
            ESP_LOGE(TAG, "esp_tls_conn_read  returned -0x%x", -ret);
            return false;
        }

        len += ret;
        // if (ret > 0) {
        //     ESP_LOGI(TAG, "%d bytes read", ret);
        //     chunks++;
        // }
    } while (ret != 0);

    // ESP_LOGI(TAG, "connection closed");

    assert(len < ARRAY_COUNT(readBuffer));
    readBuffer[len] = '\0';

    // ESP_LOGI(TAG, "%d bytes read", len);
    // ESP_LOGI(TAG, "%d bytes read in %d chunks", len, chunks);

    char* statusEnd       = strchr(readBuffer, '\r');
    bool  successResponse = strncmp(readBuffer, "HTTP/1.1 200 OK", statusEnd - readBuffer) == 0;
    if (!successResponse) {
        *statusEnd = '\0';
        ESP_LOGE(TAG, "Status code not 200 OK.");
        // Print response directly to stdout (don't require a large printf buffer)
        // for (int i = 0; i < len; i++) {
        //     putchar(readBuffer[i]);
        // }
        return false;
    }

    // char* pStart = readBuffer;
    // while (*pStart && (*pStart < 0x20))
    //     pStart++;
    const char* headerTerminator = "\r\n\r\n";
    char*       pBodyStart       = strstr(readBuffer, headerTerminator) + strlen(headerTerminator);

    // int bodyLen = strlen(pBodyStart);
    // ESP_LOGI(TAG, "Body length: %d bytes", bodyLen);
    // printf("Raw body: @@@@");
    // // Print response directly to stdout (don't require a large printf buffer)
    // for (int i = 0; i < bodyLen; i++) {
    //     putchar(pBodyStart[i]);
    // }
    // printf("@@@@\n\n\n");

    JsonObject& root = jsonBuffer.parseObject(pBodyStart);
    if (root.success()) {
        // https://arduinojson.org/v5/doc/decoding/
        // https://arduinojson.org/v5/api/jsonobject/
        // https://arduinojson.org/v5/api/jsonvariant/
        // https://arduinojson.org/v5/api/jsonarray/
        // https://arduinojson.org/v5/example/http-client/

        if (root.containsKey("status")) {
            *pResult = strcmp(root["status"].as<const char*>(), "open") == 0;
        } else {
            ESP_LOGE(TAG, "JSON data missing expected fields.");

            // printf("Raw body: @@@@");
            // // Print response directly to stdout (don't require a large printf buffer)
            // for (int i = 0; i < bodyLen; i++) {
            //     putchar(pBodyStart[i]);
            // }
            // printf("@@@@\n\n\n");

            return false;
        }
    } else {
        ESP_LOGE(TAG, "Unknown data format (not JSON).");

        // printf("Raw body: @@@@");
        // // Print response directly to stdout (don't require a large printf buffer)
        // for (int i = 0; i < bodyLen; i++) {
        //     putchar(pBodyStart[i]);
        // }
        // printf("@@@@\n\n\n");

        return false;
    }

    return true;
}

static bool https_request(esp_tls_cfg_t* pCfg, const char* web_url, const char* header, const char* body, bool* pResult) {
    *pResult = false;

    struct esp_tls* tls = esp_tls_conn_http_new(web_url, pCfg);
    if (tls == NULL) {
        ESP_LOGE(TAG, "Connection failed.");
        esp_tls_conn_delete(tls);
        return false;
    }

    // ESP_LOGI(TAG, "Connection established.");

    // printf("\n\n\n");

    if (!send_request(tls, header, body)) {
        ESP_LOGE(TAG, "Request send failed.");
        esp_tls_conn_delete(tls);
        return false;
    }
    if (!read_response(tls, pResult)) {
        ESP_LOGE(TAG, "Request read failed.");
        esp_tls_conn_delete(tls);
        return false;
    }

    // printf("\n\n\n");

    esp_tls_conn_delete(tls);
    return true;
}

static void is_vhs_open_http_task(void* pvParameters) {
    esp_tls_cfg_t cfg = {
        .alpn_protos      = NULL,
        .cacert_pem_buf   = is_vhs_open_root_cert_pem_start,
        .cacert_pem_bytes = (uint32_t)is_vhs_open_root_cert_pem_end - (uint32_t)is_vhs_open_root_cert_pem_start,
        .non_block        = false
    };

    while (1) {
        IsVHSOpenHttpNotification httpNotification = IS_VHS_OPEN_HTTP_NOTIFICATION_None;
        if (xTaskNotifyWait(0, 0, (uint32_t*)&httpNotification, 1000 / portTICK_PERIOD_MS) == pdPASS) {
            MainNotificationArgs mainNotificationArgs;
            bzero(&mainNotificationArgs, sizeof(MainNotificationArgs));
            mainNotificationArgs.notification                                = MAIN_NOTIFICATION_IsVHSOpenHttpRequestResultReady;
            mainNotificationArgs.IsVHSOpenHttpRequestResult.httpNotification = httpNotification;

            if (httpNotification == IS_VHS_OPEN_HTTP_NOTIFICATION_Status) {
                mainNotificationArgs.IsVHSOpenHttpRequestResult.success = https_request(&cfg, WEB_URL_STATUS, REQUEST_STATUS, NULL, &mainNotificationArgs.IsVHSOpenHttpRequestResult.open);
            } else {
                mainNotificationArgs.IsVHSOpenHttpRequestResult.success = false;
                ESP_LOGE(TAG, "Unknown IsVHSOpenHttpNotification: %d", (int)httpNotification);
            }

            if (xQueueSendToBack(MAIN_queueHandle, &mainNotificationArgs, 100 / portTICK_PERIOD_MS) != pdTRUE) {
                // Erk. Did not add to the queue. Oh well? User can just try again when they realize...
            }
        }
    }
}

//
void is_vhs_open_http_thread_create() {
    xTaskCreate(&is_vhs_open_http_task, "is_vhs_open_http_task", 6 * 1024, NULL, 5, &IsVHSOpenHttpTaskHandle);
}
