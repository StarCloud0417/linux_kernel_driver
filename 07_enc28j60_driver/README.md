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
| SPI kernel API | `spi_write_then_read`, `spi_sync`, `spi_message` |
| `net_device` | Linux 網路裝置的核心結構 |
| `sk_buff` | 核心封包緩衝區，TX/RX 都靠它 |
| `ndo_*` ops | open, stop, start_xmit 實作 |
| NAPI | 現代 RX polling 機制，取代純 interrupt |
| 中斷驅動 RX | `request_irq` + IRQ handler |
| ENC28J60 暫存器 | Bank switching, SRAM buffer 配置 |

## 實作進度

- [x] SPI driver 骨架 + probe/remove
- [x] net_device 結構與 ndo_ops 骨架
- [x] 中斷 handler 骨架
- [ ] SPI register read/write helpers
- [ ] 晶片初始化 (soft reset, MAC config, RX filter)
- [ ] TX path (sk_buff → chip SRAM → TXRTS)
- [ ] RX path (interrupt → read SRAM → netif_rx)
- [ ] NAPI 整合
- [ ] PHY link status (MISTAT)
- [ ] Device Tree overlay

## 參考資料

- ENC28J60 Datasheet: https://ww1.microchip.com/downloads/en/devicedoc/39662e.pdf
- 官方 kernel driver（拿來對照用）: `drivers/net/ethernet/microchip/enc28j60.c`
- sk_buff 結構: `include/linux/skbuff.h`
