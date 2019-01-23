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

#include "nomos_http_thread.h"
#include "main_thread.h"


#define TAG "NOMOS"


TaskHandle_t NomosHttpTaskHandle = NULL;

char nomosHttpRequestBody[64] = {};


#include "nomos_cert.h"
#include "nomos_api_key.h"


#define WEB_SERVER "membership.vanhack.ca"
#define WEB_PORT "443"
#define WEB_URL_VALIDATE "https://membership.vanhack.ca/services/web/MemberCardService1.svc/ValidateGenuineCard"
#define WEB_URL_CHECK_RFID "https://membership.vanhack.ca/services/web/AuthService1.svc/CheckRfid"
#define WEB_URL_CHECK_PIN "https://membership.vanhack.ca/services/web/AuthService1.svc/CheckPin"
#define WEB_URL_USER "https://membership.vanhack.ca/services/web/UserService1.svc/GetUser"

static const char* REQUEST_VALIDATE = "POST " WEB_URL_VALIDATE " HTTP/1.0\r\n"
                                      "Host: " WEB_SERVER "\r\n"
                                      "X-Api-Key: " NOMOS_API_KEY "\r\n"
                                      "User-Agent: esp-idf/1.0 esp32\r\n"
                                      "Content-Type: text/json\r\n"
                                      "Content-Length: %d\r\n"
                                      "\r\n"
                                      "%s";

static const char* REQUEST_CHECK_RFID = "POST " WEB_URL_CHECK_RFID " HTTP/1.0\r\n"
                                        "Host: " WEB_SERVER "\r\n"
                                        "X-Api-Key: " NOMOS_API_KEY "\r\n"
                                        "User-Agent: esp-idf/1.0 esp32\r\n"
                                        "Content-Type: text/json\r\n"
                                        "Content-Length: %d\r\n"
                                        "\r\n"
                                        "%s";

static const char* REQUEST_CHECK_PIN = "POST " WEB_URL_CHECK_PIN " HTTP/1.0\r\n"
                                       "Host: " WEB_SERVER "\r\n"
                                       "X-Api-Key: " NOMOS_API_KEY "\r\n"
                                       "User-Agent: esp-idf/1.0 esp32\r\n"
                                       "Content-Type: text/json\r\n"
                                       "Content-Length: %d\r\n"
                                       "\r\n"
                                       "%s";

// static const char *REQUEST_USER =
//     "POST " WEB_URL_USER " HTTP/1.0\r\n"
//     "Host: " WEB_SERVER "\r\n"
//     "X-Api-Key: " NOMOS_API_KEY "\r\n"
//     "User-Agent: esp-idf/1.0 esp32\r\n"
//     "Content-Type: text/json\r\n"
//     "Content-Length: %d\r\n"
//     "\r\n"
//     "%s";

static char                       requestBuff[512];
static char                       readBuffer[4 * 1024];
static StaticJsonBuffer<8 * 1024> jsonBuffer; // NOTE: 4*1024 would sometimes not be enough, and the parsing would fail. Sometimes...

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

static bool read_response(struct esp_tls* tls, NomosHttpResponseType responseType, NomosHttpResponseResult* pResult) {
    // char json[] = R"!({"valid":true,"type":"vhs_membership_keyholder","privileges":[],"userId":123,"username":"Foo.Bar"})!";
    // JsonObject& f = jsonBuffer.parseObject(json);
    // assert(f.success());

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

    if (responseType == NOMOS_RT_JSON) {
        JsonObject& root = jsonBuffer.parseObject(pBodyStart);
        if (root.success()) {
            // https://arduinojson.org/v5/doc/decoding/
            // https://arduinojson.org/v5/api/jsonobject/
            // https://arduinojson.org/v5/api/jsonvariant/
            // https://arduinojson.org/v5/api/jsonarray/
            // https://arduinojson.org/v5/example/http-client/

            if (root.containsKey("userId") && root.containsKey("username") &&
                root.containsKey("valid") && root.containsKey("privileges")) {
                pResult->userId     = root["userId"].as<uint32_t>();
                pResult->bValidUser = root["valid"].as<bool>();

                const JsonArray& privileges = root["privileges"].as<const JsonArray&>();
                if (privileges != JsonArray::invalid()) {
                    for (auto privilege : privileges) {
                        // JsonObject& priv = privilege.as<JsonObject>();
                        // printf("Privilege %s: %s\n", (priv.containsKey("code")) ? priv["code"].as<const char *>() : "n/a", (priv.containsKey("enabled")) ? ((priv["enabled"].as<bool>()) ? "true" : "false") : "n/a");

                        if (strcmp(privilege["code"].as<const char*>(), "door") == 0) {
                            if (privilege["enabled"].as<bool>() == true) {
                                pResult->bHasDoorAccess = true;
                            }
                        } else if (strcmp(privilege["code"].as<const char*>(), "vetted") == 0) {
                            if (privilege["enabled"].as<bool>() == true) {
                                pResult->bHasBeenVetted = true;
                            }
                        }
                    }
                }

                // const char *userName = root["username"];
                // ESP_LOGI(TAG, "User %s %s door access and is %s.",
                //               (userName == NULL) ? "(unknown)" : userName,
                //               (hasDoorAccess) ? "has" : "does not have",
                //               (isVetted) ? "vetted" : "not vetted");
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
    } else if (responseType == NOMOS_RT_BOOLEAN) {
        bool bValue = strncmp(pBodyStart, "true", 4) == 0;
        // printf("Response: %s\n", (bValue) ? "true" : "false");

        pResult->bValue = bValue;
    }

    return true;
}

static bool https_request(esp_tls_cfg_t* pCfg, NomosHttpResponseType responseType, const char* web_url, const char* header, const char* body, NomosHttpResponseResult* pResult) {
    bzero(pResult, sizeof(NomosHttpResponseResult));

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
    if (!read_response(tls, responseType, pResult)) {
        ESP_LOGE(TAG, "Request read failed.");
        esp_tls_conn_delete(tls);
        return false;
    }

    // printf("\n\n\n");

    esp_tls_conn_delete(tls);
    return true;
}

// static char taskListOutput[40*16] = { 0 };

static void nomos_https_task(void* pvParameters) {
    esp_tls_cfg_t cfg = {
        .alpn_protos      = NULL,
        .cacert_pem_buf   = nomos_root_cert_pem_start,
        .cacert_pem_bytes = (uint32_t)nomos_root_cert_pem_end - (uint32_t)nomos_root_cert_pem_start,
        .non_block        = false
    };

    while (1) {
        NomosHttpNotification httpNotification = NOMOS_HTTP_NOTIFICATION_None;
        if (xTaskNotifyWait(0, 0, (uint32_t*)&httpNotification, 1000 / portTICK_PERIOD_MS) == pdPASS) {
            MainNotificationArgs mainNotificationArgs;
            bzero(&mainNotificationArgs, sizeof(MainNotificationArgs));
            mainNotificationArgs.notification                            = MAIN_NOTIFICATION_NomosHttpRequestResultReady;
            mainNotificationArgs.NomosHttpRequestResult.httpNotification = httpNotification;

            if (httpNotification == NOMOS_HTTP_NOTIFICATION_RequestValidate) {
                // ESP_LOGI(TAG, "Validating: %s", nomosHttpRequestBody);
                mainNotificationArgs.NomosHttpRequestResult.success = https_request(&cfg, NOMOS_RT_BOOLEAN, WEB_URL_VALIDATE, REQUEST_VALIDATE, nomosHttpRequestBody, &mainNotificationArgs.NomosHttpRequestResult.result);
            } else if (httpNotification == NOMOS_HTTP_NOTIFICATION_RequestRfid) {
                // ESP_LOGI(TAG, "Checking RFID: %s", nomosHttpRequestBody);
                mainNotificationArgs.NomosHttpRequestResult.success = https_request(&cfg, NOMOS_RT_JSON, WEB_URL_CHECK_RFID, REQUEST_CHECK_RFID, nomosHttpRequestBody, &mainNotificationArgs.NomosHttpRequestResult.result);
            } else if (httpNotification == NOMOS_HTTP_NOTIFICATION_RequestPin) {
                // ESP_LOGI(TAG, "Checking Pin: %s", nomosHttpRequestBody);
                mainNotificationArgs.NomosHttpRequestResult.success = https_request(&cfg, NOMOS_RT_JSON, WEB_URL_CHECK_PIN, REQUEST_CHECK_PIN, nomosHttpRequestBody, &mainNotificationArgs.NomosHttpRequestResult.result);
            } else {
                mainNotificationArgs.NomosHttpRequestResult.success = false;
                ESP_LOGE(TAG, "Unknown NomosHttpNotification: %d", (int)httpNotification);
            }

            if (xQueueSendToBack(MAIN_queueHandle, &mainNotificationArgs, 100 / portTICK_PERIOD_MS) != pdTRUE) {
                // Erk. Did not add to the queue. Oh well? User can just try again when they realize...
            }
        }
        /* Wait for the callback to set the CONNECTED_BIT in the
           event group.
        */
        // xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
        //                     false, true, portMAX_DELAY);
        // ESP_LOGI(TAG, "Starting HTTPS tests (cert is %u bytes)...", nomos_root_cert_pem_end - nomos_root_cert_pem_start);

        // ESP_LOGI(TAG, "\n\nValidating...");
        // https_request(&cfg, NOMOS_RT_BOOLEAN, WEB_URL_VALIDATE, REQUEST_VALIDATE, "{ \"key\": \"04:50:8C:EA:50:49:80\" }");

        // ESP_LOGI(TAG, "Checking RFID...");
        // https_request(&cfg, NOMOS_RT_JSON, WEB_URL_CHECK_RFID, REQUEST_CHECK_RFID, "{ \"rfid\": \"04:50:8C:EA:50:49:80\" }");

        // ESP_LOGI(TAG, "Checking Pin...");
        // https_request(&cfg, NOMOS_RT_JSON, WEB_URL_CHECK_PIN, REQUEST_CHECK_PIN, "{ \"pin\": \"134679\" }");

        // ESP_LOGI(TAG, "User...");
        // https_request(&cfg, NOMOS_RT_JSON, WEB_URL_USER, REQUEST_USER, "{ \"userid\": 46 }");

        //
        // vTaskList(taskListOutput);
        // printf("Task List:\n%s\n\n", taskListOutput);

        // vTaskDelay(50000 / portTICK_PERIOD_MS);
        // for (int countdown = 10; countdown >= 0; countdown--)
        // {
        //     ESP_LOGI(TAG, "%d...", countdown);
        //     vTaskDelay(1000 / portTICK_PERIOD_MS);
        // }
        // ESP_LOGI(TAG, "Starting again!");
    }
}

//
void nomos_http_thread_create() {
    xTaskCreate(&nomos_https_task, "nomos_https_task", 6 * 1024, NULL, 5, &NomosHttpTaskHandle);
}
