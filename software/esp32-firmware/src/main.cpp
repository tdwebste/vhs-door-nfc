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


#define SDA_GPIO GPIO_NUM_13 //GPIO_NUM_21    //GPIO_NUM_16
#define SCL_GPIO GPIO_NUM_16 //GPIO_NUM_22    //GPIO_NUM_17

i2c_dev_t i2c_ds3231;


int init_ethernet() {
    tcpip_adapter_ip_info_t ip;
    uint8_t                 timeout = 0;

    /* Clear ip info */
    memset(&ip, 0, sizeof(tcpip_adapter_ip_info_t));

    /* Initialize ethernet */
    printf("Initialize LAN...");
    fflush(stdout);
    if (initEthernet() != ESP_OK) {
        printf("[ \033[31mERROR\033[0m ]\n");
        esp_eth_disable();
        return ESP_FAIL;
    }
    printf("[ \033[32mDONE\033[0m ]\n");

    /* Wait for IP */
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

    // wait for time to be set
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
        // update 'now' variable with current time
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


// static void rfid_task(void *pvParameters)
// {
//     /* Configure parameters of an UART driver,
//      * communication pins and install the driver */
//     uart_config_t uart_config = {
//         .baud_rate = 38400,
//         .data_bits = UART_DATA_8_BITS,
//         .parity = UART_PARITY_DISABLE,
//         .stop_bits = UART_STOP_BITS_1,
//         .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
//         .rx_flow_ctrl_thresh = 0,
//         .use_ref_tick = false};
//     ESP_ERROR_CHECK(uart_param_config(UART_NUM_1, &uart_config));
//     ESP_ERROR_CHECK(uart_set_pin(UART_NUM_1, RFID_UART_TXD, RFID_UART_RXD, RFID_UART_RTS, RFID_UART_CTS));
//     ESP_ERROR_CHECK(uart_driver_install(UART_NUM_1, RFID_UART_BUFFER_SIZE * 2, 0, 0, NULL, 0));

//     const char* rfid_eeprom_read_cmd = "e1\r\ner0,39\r\n";
//     uart_write_bytes(UART_NUM_1, (const char*)rfid_eeprom_read_cmd, strlen(rfid_eeprom_read_cmd));

//     while (1)
//     {
//         // Read data from the UART
//         int len = uart_read_bytes(UART_NUM_1, (uint8_t*)rfidUartBuffer, RFID_UART_BUFFER_SIZE - 1, 20 / portTICK_RATE_MS);
//         if (len > 0)
//         {
//             rfidUartBuffer[len] = '\0';
//             // char rfidData[512] = {};
//             // *rfidData = '\0';
//             // for (int i = 0; i < len; i++)
//             // {
//             //     char data[8];
//             //     sprintf(data, "%02x", rfidUartBuffer[i]);
//             //     strcat(rfidData, data);
//             //     if (i != (len - 1))
//             //     {
//             //         strcat(rfidData, ":");
//             //     }
//             // }
//             // ESP_LOGI("RFID", "%d bytes received: %s", len, rfidData);
//             ESP_LOGI("RFID", "%d bytes received", len);
//             printf(rfidUartBuffer);
//             printf("\n");
//         }
//         // Write data back to the UART
//         // uart_write_bytes(UART_NUM_1, (const char *) data, len);
//     }
// }

extern "C" {
void app_main(void) {
    // uint8_t ch;
    // uint16_t timeout_ms;

    // printf("-------------------------------------\n");
    // printf("Press \033[1mENTER\033[0m to start the demo...\n");
    // UART_WAIT_KEY();

    // do {
    //     printf("\n\033[1m========== Ethernet Demo ==========\033[0m\n");
    //     printf("Start...");
    //     fflush(stdout);
    //     UART_FLUSH();

    //     timeout_ms = 1000;
    //     while(--timeout_ms && uart_rx_one_char(&ch) != ESP_OK)
    //         vTaskDelay(1 / portTICK_PERIOD_MS);
    //     if(timeout_ms) {
    //         printf("[ \033[35m------\033[0m ]\n");
    //     } else {
    //         printf("[ \033[32mDONE\033[0m ]\n");
    //         testEthernet();
    //     }
    //     UART_FLUSH();
    //     printf("Press \033[1mENTER\033[0m to finish or \033[1mR\033[0m to repeat...\n");
    //     UART_WAIT_KEY();
    // } while((ch == 'r' || ch == 'R'));

    // while(1)
    //     vTaskDelay(1000 / portTICK_PERIOD_MS);

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

    //
    // esp_eth_disable();
}
}
