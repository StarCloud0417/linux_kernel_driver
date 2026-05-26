# ESP32 WiFi Coprocessor Driver

ESP32 當 WiFi 協處理器，Raspberry Pi 透過 SPI 下指令，實現完整的 Linux WiFi 驅動。

## 架構

```
RPi Linux                          ESP32
─────────────────────────────      ──────────────────────────
[ Application / Socket ]
         ↓
[ mac80211 / cfg80211 ]
         ↓
[ This Driver ]     ←── SPI ──→   [ esp32_firmware (FreeRTOS) ]
                                            ↓
                                   [ ESP-IDF WiFi Stack ]
                                            ↓
                                   [ 802.11 RF ]
```

## SPI 封包協定

```
┌────────┬────────┬─────────┬──────────────┐
│ MAGIC  │  TYPE  │   LEN   │   PAYLOAD    │
│ 2 byte │ 1 byte │ 2 byte  │   N bytes    │
└────────┴────────┴─────────┴──────────────┘

TYPE:
  0x01 = DATA  (Ethernet frame)
  0x02 = CMD   (Host → ESP32 控制命令)
  0x03 = EVENT (ESP32 → Host 事件回報)
```

## 硬體接線

```
RPi                ESP32
───────────        ─────
GPIO10 (MOSI) →   GPIO13
GPIO9  (MISO) →   GPIO12
GPIO11 (CLK)  →   GPIO14
GPIO8  (CS0)  →   GPIO15
GPIO25 (INT)  →   GPIO4   ← ESP32 通知 RPi 有資料
GPIO24 (RST)  →   EN      ← RPi 重置 ESP32
3.3V          →   3.3V
GND           →   GND
```

## 目錄結構

```
08_esp32_wifi_driver/
├── linux_driver/           ← RPi 這端的 kernel module
│   ├── esp32_wifi_driver.c
│   └── Makefile
└── esp32_firmware/         ← ESP32 這端的 FreeRTOS firmware
    └── main.c
```

## 涵蓋的核心概念

| 概念 | 說明 |
|:-----|:-----|
| `mac80211` | Linux WiFi 子系統，處理 802.11 協定 |
| `ieee80211_hw` / `ieee80211_ops` | WiFi driver 的核心結構 |
| `ieee80211_rx_irqsafe` | 把收到的封包交給 mac80211 |
| `hw_scan` | 觸發 WiFi 掃描，回傳 BSS 列表 |
| SPI Slave (ESP-IDF) | ESP32 端的 SPI 接收實作 |
| `esp_wifi_internal_tx` | ESP-IDF 直接送出 WiFi 封包的 API |
| FreeRTOS Task | ESP32 端的並發處理 |

## 實作進度

**Linux Driver (RPi):**
- [x] `ieee80211_hw` + `ieee80211_ops` 骨架
- [x] SPI probe/remove
- [x] 2.4GHz 頻段與速率表
- [x] RX workqueue 架構
- [ ] SPI 封包收發實作
- [ ] `esp32_op_tx` 完整實作
- [ ] `esp32_op_hw_scan` + scan result 回傳
- [ ] `bss_info_changed` → connect CMD
- [ ] Device Tree overlay

**ESP32 Firmware:**
- [x] 封包格式定義
- [x] app_main 骨架
- [ ] SPI Slave 初始化
- [ ] WiFi stack 初始化
- [ ] SPI task 封包處理 loop
- [ ] INT pin 通知機制

## 參考資料

- espressif/esp-hosted (官方相同概念完整實作)
- `drivers/net/wireless/ath/ath9k/htc_drv_*.c` (最乾淨的 mac80211 driver 範例)
- ESP-IDF SPI Slave 文件: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/spi_slave.html
- mac80211 開發者文件: https://wireless.wiki.kernel.org/en/developers/documentation/mac80211
