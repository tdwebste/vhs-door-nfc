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

#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "esp_vfs_fat.h"
#include "esp_task_wdt.h"
#include "esp_eth.h"
#include "esp_log.h"
#include "nvs_flash.h"

extern "C" {
#include <olimex_ethernet.h>
}

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"
#include "apps/sntp/sntp.h"

#include "esp_tls.h"

#include <i2cdev.h>
#include <ds3231.h>

#include "utils.h"

#include "main_thread.h"
#include "uart_thread.h"
#include "nomos_http_thread.h"
#include "is_vhs_open_http_thread.h"


#define TAG "NOMOS"


#define SDA_GPIO GPIO_NUM_13
#define SCL_GPIO GPIO_NUM_16

i2c_dev_t i2c_ds3231;


int init_ethernet() {
    tcpip_adapter_ip_info_t ip;
    uint8_t                 timeout = 0;

    memset(&ip, 0, sizeof(tcpip_adapter_ip_info_t));

    // Initialize ethernet
    printf("Initialize LAN...");
    fflush(stdout);
    if (initEthernet() != ESP_OK) {
        printf("[ \033[31mERROR\033[0m ]\n");
        esp_eth_disable();
        return ESP_FAIL;
    }
    printf("[ \033[32mDONE\033[0m ]\n");

    // Wait for IP
    printf("Receiving address...");
    fflush(stdout);
    while (timeout++ < 10) {
        if (tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_ETH, &ip) == 0) {
            if (ip.gw.addr != 0) { //== 0x0100a8c0)
                break;
            }
        }

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    // 0x0100a8c0: 1.0.168.192
    // 0x0101a8c0: 1.1.168.192
    if (ip.gw.addr != 0) { //== 0x0100a8c0)
        printf("[ \033[32mDONE\033[0m ]\n");
    } else {
        printf("[ \033[31mERROR\033[0m ]\n");
    }

    // SPP +
    int x = ip.ip.addr, A[4];
    A[0]  = (x >> 0) & 0xFF;
    A[1]  = (x >> 8) & 0xFF;
    A[2]  = (x >> 16) & 0xFF;
    A[3]  = (x >> 24) & 0xFF;
    printf("IP address: %d.%d.%d.%d\n", A[0], A[1], A[2], A[3]);

    return 0;
}

void init_time() {
    printf("Initializing SNTP...\n");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    // NOTE: The default configuration of ESP-IDF under PlatformIO only
    // supports one NTP server, but this has been changed to support 4
    // see: sdkconfig.ini: CONFIG_LWIP_DHCP_MAX_NTP_SERVERS
    // This should have been changed via make menuconfig, but at this
    // time PlatformIO doesn't support that.
    sntp_setservername(0, (char*)"pool.ntp.org");
    sntp_setservername(1, (char*)"time.nist.gov");
    sntp_setservername(2, (char*)"north-america.pool.ntp.org");
    sntp_setservername(3, (char*)"ca.pool.ntp.org");
    sntp_init();

    // Wait for time to be set
    time_t    now         = 0;
    struct tm timeinfo    = {};
    int       retry       = 0;
    const int retry_count = 10;
    // Is time set? If not, tm_year will be (1970 - 1900)
    while (timeinfo.tm_year < (2000 - 1900) && ++retry < retry_count) {
        printf("Waiting for system time to be set... (%d/%d)\n", retry, retry_count);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        time(&now);
        localtime_r(&now, &timeinfo);
    }

    if (timeinfo.tm_year >= (2018 - 1900)) {
        time_t now;
        time(&now);

        // Set timezone to Pacific Standard Time (America/Vancouver) and print local time
        // see: https://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html
        // see: https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
        setenv("TZ", "PST8PDT,M3.2.0,M11.1.0", 1);
        tzset();
        char strftime_buf[64];
        localtime_r(&now, &timeinfo);
        // see http://www.cplusplus.com/reference/ctime/strftime/
        // "%d-%b-%y, %H:%M:%S"
        strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
        printf("The current date/time in Vancouver is: %s\n", strftime_buf);

        localtime_r(&now, &timeinfo);
        while (ds3231_set_time(&i2c_ds3231, &timeinfo) != ESP_OK) {
            printf("Could not set time\n");
            vTaskDelay(250 / portTICK_PERIOD_MS);
        }
    } else {
        printf("Failed to update date/time via SMTP. Falling back to RTC...\n");

        float temperature = 0.0f;
        if (ds3231_get_temp_float(&i2c_ds3231, &temperature) != ESP_OK) {
            printf("Could not get temperature\n");
        }

        if (ds3231_get_time(&i2c_ds3231, &timeinfo) != ESP_OK) {
            printf("Could not get time\n");
        }

        now = mktime(&timeinfo);
        struct timeval timeVal;
        timeVal.tv_sec  = now;
        timeVal.tv_usec = 0;
        int result      = settimeofday(&timeVal, NULL);
        if (result < 0) {
            printf("Error setting the Time.\n");
        } else {
            printf("Time has been configured.\n");
        }

        // Set timezone to Pacific Standard Time (America/Vancouver) and print local time
        // see: https://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html
        // see: https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
        setenv("TZ", "PST8PDT,M3.2.0,M11.1.0", 1);
        tzset();

        char strftime_buf[64];
        strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
        printf("The current date/time in Vancouver is: %s\n", strftime_buf);
        printf("The current temperature is: %0.1f degrees C\n", temperature);
    }
}

extern "C" {
void app_main(void) {
    // Initialize NVS — it is used to store PHY calibration data
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    //
    while (i2cdev_init() != ESP_OK) {
        printf("Could not init I2Cdev library\n");
        vTaskDelay(250 / portTICK_PERIOD_MS);
    }

    //
    while (ds3231_init_desc(&i2c_ds3231, I2C_NUM_0, SDA_GPIO, SCL_GPIO) != ESP_OK) {
        printf("Could not init ds3231 device descriptor\n");
        vTaskDelay(250 / portTICK_PERIOD_MS);
    }

    //
    init_ethernet();

    //
    init_time();

    //
    uart_init();

    //
    main_thread_init();
    uart_thread_create();
    nomos_http_thread_create();
    is_vhs_open_http_thread_create();

    main_thread_run();
}
}
