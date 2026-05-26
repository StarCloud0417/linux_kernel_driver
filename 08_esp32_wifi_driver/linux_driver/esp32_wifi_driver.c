// SPDX-License-Identifier: GPL-2.0
/*
 * ESP32 WiFi Coprocessor Driver (Learning Implementation)
 *
 * Author: Frank Huang
 * Platform: Raspberry Pi 5 (Host) + ESP32 (WiFi Coprocessor)
 *
 * Architecture:
 *
 *   RPi Linux                          ESP32
 *   ─────────────────────────────      ────────────────────────
 *   [ Application / Socket Layer ]
 *            ↓
 *   [ mac80211 / cfg80211 ]
 *            ↓
 *   [ This Driver (ieee80211_hw) ] ←──SPI──→ [ esp32_firmware ]
 *                                              ↓
 *                                    [ ESP-IDF WiFi Stack ]
 *                                              ↓
 *                                    [ 802.11 RF (真實無線) ]
 *
 * SPI Packet Protocol (自訂 TLV 格式):
 *
 *   ┌────────┬────────┬─────────┬──────────────┐
 *   │ MAGIC  │  TYPE  │  LEN    │   PAYLOAD    │
 *   │ 2 byte │ 1 byte │ 2 byte  │   N bytes    │
 *   └────────┴────────┴─────────┴──────────────┘
 *
 *   TYPE:
 *     0x01 = DATA frame (封包)
 *     0x02 = CMD  (控制命令：scan, connect, disconnect)
 *     0x03 = EVENT (ESP32 → RPi 事件：link up/down, scan result)
 *
 * Wiring (SPI0):
 *   RPi GPIO10 (MOSI) → ESP32 GPIO13
 *   RPi GPIO9  (MISO) → ESP32 GPIO12
 *   RPi GPIO11 (CLK)  → ESP32 GPIO14
 *   RPi GPIO8  (CS0)  → ESP32 GPIO15
 *   RPi GPIO25 (INT)  → ESP32 GPIO4   ← ESP32 通知 RPi 有資料
 *   RPi GPIO24 (RST)  → ESP32 EN      ← RPi 重置 ESP32
 *
 * Key Concepts Covered:
 *   - mac80211 / ieee80211_hw framework
 *   - ieee80211_ops (tx, start, stop, hw_scan, sta_add...)
 *   - SPI slave 通訊協定設計
 *   - WiFi 控制流：scan → associate → 4-way handshake (WPA2)
 *   - cfg80211 channel / band 設定
 *
 * Reference:
 *   espressif/esp-hosted (官方相同概念的完整實作)
 *   drivers/net/wireless/ath/ath9k/htc_drv_*.c
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/spi/spi.h>
#include <linux/interrupt.h>
#include <linux/gpio/consumer.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/of.h>
#include <net/mac80211.h>           /* ieee80211_hw, ieee80211_ops */
#include <net/cfg80211.h>           /* wiphy, channel info */

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Frank Huang");
MODULE_DESCRIPTION("ESP32 WiFi Coprocessor Driver - Learning Implementation");
MODULE_VERSION("0.1");

/* ============================================================
 * SPI Protocol Definition
 * ============================================================ */
#define ESP32_MAGIC         0xA5A5
#define ESP32_PKT_MAX_LEN   1600

/* Packet types */
#define ESP32_TYPE_DATA     0x01    /* 802.3 Ethernet frame */
#define ESP32_TYPE_CMD      0x02    /* Control command to ESP32 */
#define ESP32_TYPE_EVENT    0x03    /* Event from ESP32 */

/* Commands (Host → ESP32) */
#define ESP32_CMD_SCAN      0x10    /* Trigger WiFi scan */
#define ESP32_CMD_CONNECT   0x11    /* Connect to AP */
#define ESP32_CMD_DISCONNECT 0x12  /* Disconnect */
#define ESP32_CMD_SET_MAC   0x13

/* Events (ESP32 → Host) */
#define ESP32_EVT_SCAN_DONE  0x20
#define ESP32_EVT_LINK_UP    0x21
#define ESP32_EVT_LINK_DOWN  0x22
#define ESP32_EVT_RX_READY   0x23   /* ESP32 has a packet waiting */

/**
 * struct esp32_pkt_hdr - SPI packet header (little-endian)
 */
struct esp32_pkt_hdr {
    __le16 magic;
    u8     type;
    __le16 len;
} __packed;

/* ============================================================
 * 2.4GHz Channel Table (802.11b/g/n)
 * ============================================================ */
static struct ieee80211_channel esp32_2ghz_channels[] = {
    /* ch1  */ { .band = NL80211_BAND_2GHZ, .center_freq = 2412, .hw_value = 1 },
    /* ch2  */ { .band = NL80211_BAND_2GHZ, .center_freq = 2417, .hw_value = 2 },
    /* ch3  */ { .band = NL80211_BAND_2GHZ, .center_freq = 2422, .hw_value = 3 },
    /* ch4  */ { .band = NL80211_BAND_2GHZ, .center_freq = 2427, .hw_value = 4 },
    /* ch5  */ { .band = NL80211_BAND_2GHZ, .center_freq = 2432, .hw_value = 5 },
    /* ch6  */ { .band = NL80211_BAND_2GHZ, .center_freq = 2437, .hw_value = 6 },
    /* ch7  */ { .band = NL80211_BAND_2GHZ, .center_freq = 2442, .hw_value = 7 },
    /* ch8  */ { .band = NL80211_BAND_2GHZ, .center_freq = 2447, .hw_value = 8 },
    /* ch9  */ { .band = NL80211_BAND_2GHZ, .center_freq = 2452, .hw_value = 9 },
    /* ch10 */ { .band = NL80211_BAND_2GHZ, .center_freq = 2457, .hw_value = 10 },
    /* ch11 */ { .band = NL80211_BAND_2GHZ, .center_freq = 2462, .hw_value = 11 },
    /* ch12 */ { .band = NL80211_BAND_2GHZ, .center_freq = 2467, .hw_value = 12 },
    /* ch13 */ { .band = NL80211_BAND_2GHZ, .center_freq = 2472, .hw_value = 13 },
};

static struct ieee80211_rate esp32_rates[] = {
    { .bitrate = 10,  .hw_value = 0 },  /* 1 Mbps */
    { .bitrate = 20,  .hw_value = 1 },  /* 2 Mbps */
    { .bitrate = 55,  .hw_value = 2 },  /* 5.5 Mbps */
    { .bitrate = 110, .hw_value = 3 },  /* 11 Mbps */
    { .bitrate = 60,  .hw_value = 4 },  /* 6 Mbps (OFDM) */
    { .bitrate = 90,  .hw_value = 5 },
    { .bitrate = 120, .hw_value = 6 },
    { .bitrate = 180, .hw_value = 7 },
    { .bitrate = 240, .hw_value = 8 },
    { .bitrate = 360, .hw_value = 9 },
    { .bitrate = 480, .hw_value = 10 },
    { .bitrate = 540, .hw_value = 11 },
};

static struct ieee80211_supported_band esp32_band_2ghz = {
    .channels   = esp32_2ghz_channels,
    .n_channels = ARRAY_SIZE(esp32_2ghz_channels),
    .bitrates   = esp32_rates,
    .n_bitrates = ARRAY_SIZE(esp32_rates),
};

/* ============================================================
 * Driver Private Data
 * ============================================================ */
struct esp32_priv {
    struct spi_device       *spi;
    struct ieee80211_hw     *hw;        /* mac80211 handle */
    struct wiphy            *wiphy;

    struct gpio_desc        *gpio_int;  /* INT: ESP32 → RPi data ready */
    struct gpio_desc        *gpio_rst;  /* RST: RPi → ESP32 reset */
    int                      irq;

    struct work_struct       rx_work;   /* RX processing in workqueue */
    struct workqueue_struct *wq;

    struct mutex             spi_lock;
    bool                     running;
};

/* ============================================================
 * SPI Transport
 * ============================================================ */

/**
 * esp32_spi_send - Send a packet to ESP32
 * @priv: driver private
 * @type: ESP32_TYPE_DATA / CMD
 * @payload: data buffer
 * @len: payload length
 */
static int esp32_spi_send(struct esp32_priv *priv, u8 type,
                           const void *payload, u16 len)
{
    /* TODO:
     * 1. Build esp32_pkt_hdr (magic, type, len)
     * 2. Allocate spi_message + 2 spi_transfers (header + payload)
     * 3. mutex_lock(&priv->spi_lock)
     * 4. spi_sync(priv->spi, &msg)
     * 5. mutex_unlock
     */
    return 0;
}

/**
 * esp32_spi_recv - Read a packet from ESP32
 * Should be called from rx_work after INT fires.
 */
static int esp32_spi_recv(struct esp32_priv *priv, u8 *buf, u16 *out_len, u8 *out_type)
{
    /* TODO:
     * 1. Read header (5 bytes)
     * 2. Validate magic
     * 3. Read payload (header.len bytes)
     */
    return 0;
}

/* ============================================================
 * RX Workqueue Handler
 * ============================================================ */
static void esp32_rx_work(struct work_struct *work)
{
    struct esp32_priv *priv = container_of(work, struct esp32_priv, rx_work);
    u8 buf[ESP32_PKT_MAX_LEN];
    u16 len;
    u8 type;

    esp32_spi_recv(priv, buf, &len, &type);

    switch (type) {
    case ESP32_TYPE_DATA:
        /*
         * TODO: Wrap raw ethernet frame in sk_buff, pass to mac80211
         *   skb = dev_alloc_skb(len)
         *   memcpy(skb_put(skb, len), buf, len)
         *   skb->protocol = eth_type_trans(skb, ...)
         *   ieee80211_rx_irqsafe(priv->hw, skb)
         */
        pr_info("esp32: RX data %d bytes\n", len);
        break;

    case ESP32_TYPE_EVENT:
        /* TODO: handle scan results, link up/down */
        pr_info("esp32: event received\n");
        break;
    }
}

/* ============================================================
 * Interrupt Handler — INT pin goes low when ESP32 has data
 * ============================================================ */
static irqreturn_t esp32_irq_handler(int irq, void *dev_id)
{
    struct esp32_priv *priv = dev_id;
    queue_work(priv->wq, &priv->rx_work);
    return IRQ_HANDLED;
}

/* ============================================================
 * mac80211 ops — the heart of a WiFi driver
 * ============================================================ */

/**
 * esp32_op_tx - Transmit a WiFi frame
 *
 * mac80211 calls this with a tx_control and sk_buff.
 * The skb contains an 802.11 frame (not Ethernet).
 * We strip the 802.11 header down to raw data and send via SPI.
 */
static void esp32_op_tx(struct ieee80211_hw *hw,
                        struct ieee80211_tx_control *control,
                        struct sk_buff *skb)
{
    struct esp32_priv *priv = hw->priv;

    /* TODO:
     * 1. Extract payload from skb (skip 802.11 header if in managed mode)
     * 2. esp32_spi_send(priv, ESP32_TYPE_DATA, skb->data, skb->len)
     * 3. ieee80211_tx_status(hw, skb)  ← tell mac80211 TX is done
     */

    pr_info("esp32: tx %d bytes\n", skb->len);
    ieee80211_free_txskb(hw, skb);
}

/**
 * esp32_op_start - Interface brought up
 */
static int esp32_op_start(struct ieee80211_hw *hw)
{
    struct esp32_priv *priv = hw->priv;
    pr_info("esp32: mac80211 start\n");
    priv->running = true;
    /* TODO: reset ESP32, wait for ready event */
    return 0;
}

/**
 * esp32_op_stop - Interface brought down
 */
static void esp32_op_stop(struct ieee80211_hw *hw, bool suspended)
{
    struct esp32_priv *priv = hw->priv;
    pr_info("esp32: mac80211 stop\n");
    priv->running = false;
    /* TODO: send disconnect cmd to ESP32 */
}

/**
 * esp32_op_config - Channel / power config change
 */
static int esp32_op_config(struct ieee80211_hw *hw, u32 changed)
{
    /* TODO: send channel change CMD to ESP32 */
    return 0;
}

/**
 * esp32_op_hw_scan - Trigger a WiFi scan
 *
 * cfg80211 (iw scan) → mac80211 → here → SPI CMD to ESP32
 * When done, ESP32 sends EVENT_SCAN_DONE with BSS list.
 * We call ieee80211_scan_completed() when we get that event.
 */
static int esp32_op_hw_scan(struct ieee80211_hw *hw,
                             struct ieee80211_vif *vif,
                             struct ieee80211_scan_request *req)
{
    struct esp32_priv *priv = hw->priv;
    pr_info("esp32: hw_scan triggered\n");
    /* TODO: esp32_spi_send(priv, ESP32_TYPE_CMD, &scan_cmd, ...) */
    return 0;
}

/**
 * esp32_op_bss_info_changed - AP association info changed
 * Called after successful association with an AP.
 */
static void esp32_op_bss_info_changed(struct ieee80211_hw *hw,
                                       struct ieee80211_vif *vif,
                                       struct ieee80211_bss_conf *info,
                                       u64 changed)
{
    /* TODO: send CONNECT cmd with BSSID/SSID/key to ESP32 */
}

static const struct ieee80211_ops esp32_mac80211_ops = {
    .tx                 = esp32_op_tx,
    .start              = esp32_op_start,
    .stop               = esp32_op_stop,
    .config             = esp32_op_config,
    .hw_scan            = esp32_op_hw_scan,
    .bss_info_changed   = esp32_op_bss_info_changed,
};

/* ============================================================
 * SPI Probe
 * ============================================================ */
static int esp32_probe(struct spi_device *spi)
{
    struct ieee80211_hw *hw;
    struct esp32_priv   *priv;
    int ret;

    pr_info("esp32_wifi: probe\n");

    /* 1. Allocate ieee80211_hw (this also allocs our private struct) */
    hw = ieee80211_alloc_hw(sizeof(struct esp32_priv), &esp32_mac80211_ops);
    if (!hw)
        return -ENOMEM;

    priv = hw->priv;
    priv->spi = spi;
    priv->hw  = hw;
    mutex_init(&priv->spi_lock);
    spi_set_drvdata(spi, priv);

    /* 2. SPI config */
    spi->max_speed_hz  = 10000000; /* 10 MHz */
    spi->mode          = SPI_MODE_0;
    spi->bits_per_word = 8;
    ret = spi_setup(spi);
    if (ret)
        goto err_free_hw;

    /* 3. Workqueue for RX processing (SPI read can't happen in IRQ context) */
    priv->wq = create_singlethread_workqueue("esp32_wifi");
    if (!priv->wq) {
        ret = -ENOMEM;
        goto err_free_hw;
    }
    INIT_WORK(&priv->rx_work, esp32_rx_work);

    /* 4. Tell mac80211 what bands we support */
    hw->wiphy->bands[NL80211_BAND_2GHZ] = &esp32_band_2ghz;

    /* 5. Hardware capabilities */
    ieee80211_hw_set(hw, SIGNAL_DBM);
    ieee80211_hw_set(hw, HAS_RATE_CONTROL);

    /* 6. Set MAC address
     * TODO: Read from ESP32 via SPI CMD, or from DT
     */
    eth_random_addr(hw->wiphy->perm_addr);

    /* 7. Register with mac80211 */
    ret = ieee80211_register_hw(hw);
    if (ret) {
        pr_err("esp32: ieee80211_register_hw failed: %d\n", ret);
        goto err_destroy_wq;
    }

    pr_info("esp32_wifi: registered as %s\n",
            wiphy_name(hw->wiphy));
    return 0;

err_destroy_wq:
    destroy_workqueue(priv->wq);
err_free_hw:
    ieee80211_free_hw(hw);
    return ret;
}

static void esp32_remove(struct spi_device *spi)
{
    struct esp32_priv *priv = spi_get_drvdata(spi);

    ieee80211_unregister_hw(priv->hw);
    destroy_workqueue(priv->wq);
    ieee80211_free_hw(priv->hw);
    pr_info("esp32_wifi: removed\n");
}

/* ============================================================
 * Device Tree Matching
 * ============================================================ */
static const struct of_device_id esp32_of_match[] = {
    { .compatible = "espressif,esp32-wifi-coprocessor" },
    { }
};
MODULE_DEVICE_TABLE(of, esp32_of_match);

static const struct spi_device_id esp32_spi_ids[] = {
    { "esp32-wifi", 0 },
    { }
};
MODULE_DEVICE_TABLE(spi, esp32_spi_ids);

static struct spi_driver esp32_driver = {
    .driver = {
        .name           = "esp32-wifi-coprocessor",
        .of_match_table = esp32_of_match,
    },
    .id_table = esp32_spi_ids,
    .probe    = esp32_probe,
    .remove   = esp32_remove,
};

module_spi_driver(esp32_driver);
