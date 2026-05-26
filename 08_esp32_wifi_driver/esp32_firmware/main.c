/* ESP32 WiFi Coprocessor Firmware
 * Author: Frank Huang
 *
 * 這端跑在 ESP32，負責：
 *   1. 接收來自 RPi 的 SPI 封包（DATA / CMD）
 *   2. 把 DATA 丟進 ESP-IDF WiFi stack 送出去
 *   3. 收到 WiFi 封包後透過 SPI 回傳給 RPi
 *   4. 處理 CMD（scan, connect, disconnect）並回傳 EVENT
 *
 * 開發環境：ESP-IDF v5.x
 * 目標晶片：ESP32 (更新型號確認後調整)
 *
 * TODO List:
 *   [ ] SPI Slave 初始化 (spi_slave_initialize)
 *   [ ] 封包收發 loop (FreeRTOS task)
 *   [ ] WiFi 初始化 (esp_wifi_init, esp_wifi_start)
 *   [ ] CMD 處理 (scan, connect)
 *   [ ] EVENT 回傳 (link up/down, scan result)
 *   [ ] INT pin 拉低通知 RPi
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/spi_slave.h"
#include "driver/gpio.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

static const char *TAG = "esp32_wifi_cp";

/* ============================================================
 * SPI Slave 腳位定義
 * 對應 RPi 接線：
 *   MOSI → GPIO13
 *   MISO → GPIO12
 *   CLK  → GPIO14
 *   CS   → GPIO15
 *   INT  → GPIO4  (ESP32 主動拉低通知 RPi)
 * ============================================================ */
#define PIN_MOSI    13
#define PIN_MISO    12
#define PIN_CLK     14
#define PIN_CS      15
#define PIN_INT     4       /* 拉低 = 通知 RPi「我有資料」 */

/* ============================================================
 * 封包格式（與 Linux driver 端一致）
 * ============================================================ */
#define PKT_MAGIC       0xA5A5
#define PKT_MAX_LEN     1600

#define TYPE_DATA       0x01
#define TYPE_CMD        0x02
#define TYPE_EVENT      0x03

#define CMD_SCAN        0x10
#define CMD_CONNECT     0x11
#define CMD_DISCONNECT  0x12

#define EVT_SCAN_DONE   0x20
#define EVT_LINK_UP     0x21
#define EVT_LINK_DOWN   0x22
#define EVT_RX_READY    0x23

#pragma pack(1)
typedef struct {
    uint16_t magic;
    uint8_t  type;
    uint16_t len;
} pkt_hdr_t;
#pragma pack()

/* ============================================================
 * TODO: SPI Slave 初始化
 * ============================================================ */
static void spi_slave_init(void)
{
    /* TODO:
     * spi_bus_config_t buscfg = { .mosi_io_num=PIN_MOSI, ... };
     * spi_slave_interface_config_t slvcfg = { .spics_io_num=PIN_CS, ... };
     * spi_slave_initialize(SPI2_HOST, &buscfg, &slvcfg, SPI_DMA_CH_AUTO);
     */
    ESP_LOGI(TAG, "SPI Slave init (TODO)");
}

/* ============================================================
 * TODO: WiFi 初始化
 * ============================================================ */
static void wifi_init(void)
{
    /* TODO:
     * nvs_flash_init();
     * esp_netif_init();
     * esp_event_loop_create_default();
     * wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
     * esp_wifi_init(&cfg);
     * esp_wifi_set_mode(WIFI_MODE_STA);
     * esp_wifi_start();
     */
    ESP_LOGI(TAG, "WiFi init (TODO)");
}

/* ============================================================
 * TODO: 通知 RPi 有封包等待
 * ============================================================ */
static void notify_host(void)
{
    /* 拉低 INT pin → RPi 收到中斷 → 讀取封包 */
    gpio_set_level(PIN_INT, 0);
    vTaskDelay(pdMS_TO_TICKS(1));
    gpio_set_level(PIN_INT, 1);
}

/* ============================================================
 * Main SPI Task (跑在 FreeRTOS)
 * ============================================================ */
static void spi_task(void *arg)
{
    ESP_LOGI(TAG, "SPI task started");

    while (1) {
        /* TODO:
         * 1. spi_slave_transmit() 等待 RPi 發來的封包
         * 2. 解析 pkt_hdr_t
         * 3. 依 type 處理：
         *    TYPE_DATA → esp_wifi_internal_tx() 送出 WiFi
         *    TYPE_CMD  → 處理 CMD（scan/connect）
         * 4. 如果 WiFi 收到封包 → 包成 pkt_hdr_t → notify_host() → 等 RPi 來讀
         */
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/* ============================================================
 * app_main
 * ============================================================ */
void app_main(void)
{
    ESP_LOGI(TAG, "ESP32 WiFi Coprocessor Firmware starting...");

    /* INT pin 設為輸出，預設高電位 */
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIN_INT),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io_conf);
    gpio_set_level(PIN_INT, 1);

    spi_slave_init();
    wifi_init();

    xTaskCreate(spi_task, "spi_task", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "Ready.");
}
