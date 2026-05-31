# ENC28J60 SPI Ethernet Driver

學習實作：透過 SPI 介面驅動 ENC28J60 MAC+PHY 晶片，讓 Raspberry Pi 多一個有線網卡。

## 硬體接線

```
ENC28J60    RPi GPIO Header
--------    ----------------
VCC    →    Pin 1  (3.3V)
GND    →    Pin 6  (GND)
SCK    →    Pin 23 (GPIO11, SPI0_CLK)
MOSI   →    Pin 19 (GPIO10, SPI0_MOSI)
MISO   →    Pin 21 (GPIO9,  SPI0_MISO)
CS     →    Pin 24 (GPIO8,  SPI0_CE0)
INT    →    Pin 22 (GPIO25)
```

## 涵蓋的核心概念

| 概念 | 說明 |
|:-----|:-----|
| SPI kernel API | `spi_write_then_read`, `spi_write`, `spi_sync` |
| Register 地址編碼 | bits[4:0]=addr, bits[6:5]=bank, bit[7]=SPRD_MASK（MAC/MII dummy byte）|
| Bank 切換 | ECON1[1:0] 控制，priv->bank 做 cache，只改有差異的 bit |
| Soft Reset Errata | SRC(0xFF) 後 udelay(2000)，不靠 CLKRDY（B7 silicon errata）|
| `net_device` | Linux 網路裝置的核心結構 |
| `sk_buff` | 核心封包緩衝區，TX/RX 都靠它 |
| `ndo_*` ops | open, stop, start_xmit 實作 |
| NAPI | 現代 RX polling 機制，取代純 interrupt |
| 中斷驅動 RX | `request_irq` + IRQ handler |

## 實作進度

- [x] Step 1：SPI driver 骨架 + probe/remove（`module_spi_driver` 註冊）
- [x] Step 2a：Register helpers（spi_write_op / spi_read_op / set_bank）+ EREVID 驗證
- [ ] Step 2b：enc28j60_hw_init()（RX/TX buffer、MACON、MAC addr、開 RXEN）
- [ ] Step 3：net_device 註冊 + open / stop → eth1 出現
- [ ] Step 4：TX 路徑（sk_buff → SRAM → TXRTS）
- [ ] Step 5：IRQ + NAPI RX → ping 通

## Kernel 6.12 API 差異（踩過的坑）

| 舊 API | 新 API（6.12）| 說明 |
|:-------|:--------------|:-----|
| `spi->master` | `spi->controller` | master 已改名 controller |
| `spi->chip_select` | `spi_get_chipselect(spi, 0)` | 改用 helper 取 CS |

## 官方參考來源

遇到問題、不確定做法時，優先查閱：

- **官方 kernel driver（最重要）**:
  https://github.com/torvalds/linux/blob/master/drivers/net/ethernet/microchip/
- ENC28J60 Datasheet: https://ww1.microchip.com/downloads/en/devicedoc/39662e.pdf
- sk_buff 結構: `include/linux/skbuff.h`
- SPI subsystem: `include/linux/spi/spi.h`

> 遇到 register 操作、初始化順序、errata 等問題，直接翻官方 enc28j60.c 和 enc28j60_hw.h，不要盲目迭代。
