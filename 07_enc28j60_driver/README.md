# ENC28J60 SPI Ethernet Driver

學習實作：透過 SPI 介面驅動 ENC28J60 MAC+PHY 晶片，讓 Raspberry Pi 5 多一個有線網卡（eth1）。

> **遇到問題優先查閱官方實作：**
> https://github.com/torvalds/linux/blob/master/drivers/net/ethernet/microchip/
> 不要盲目迭代，直接翻 `enc28j60.c` 和 `enc28j60_hw.h`。

---

## 目錄

1. [硬體接線](#硬體接線)
2. [DTS Overlay 設定](#dts-overlay-設定)
3. [Build & 載入驅動](#build--載入驅動)
4. [測試方式（每個 Step）](#測試方式)
5. [實作進度](#實作進度)
6. [Kernel 6.12 API 差異（踩坑紀錄）](#kernel-612-api-差異)
7. [核心概念筆記](#核心概念筆記)
8. [參考資料](#參考資料)

---

## 硬體接線

```
ENC28J60    RPi 5 GPIO Header
--------    ----------------
VCC    →    Pin 1  (3.3V)
GND    →    Pin 6  (GND)
SCK    →    Pin 23 (GPIO11, SPI0_CLK)
MOSI   →    Pin 19 (GPIO10, SPI0_MOSI)
MISO   →    Pin 21 (GPIO9,  SPI0_MISO)
CS     →    Pin 24 (GPIO8,  SPI0_CE0)
INT    →    Pin 22 (GPIO25)   ← 中斷腳，下降沿觸發
```

---

## DTS Overlay 設定

### 說明

ENC28J60 必須透過 Device Tree Overlay 告訴 kernel 這個 SPI 裝置在哪、用哪個 GPIO 中斷。
不加 DTS，kernel 不會呼叫 probe()，驅動完全不會啟動。

### 檔案：`enc28j60-overlay.dts`

```dts
/dts-v1/;
/plugin/;

/ {
    compatible = "brcm,bcm2835";
};

&spi0 {
    #address-cells = <1>;
    #size-cells = <0>;
    status = "okay";

    enc28j60: enc28j60@0 {
        compatible = "microchip,enc28j60";  /* 必須和 driver 的 of_device_id 一致 */
        reg = <0>;                          /* CS0 → SPI0_CE0 (Pin 24) */
        pinctrl-names = "default";
        pinctrl-0 = <&eth1_pins>;
        interrupt-parent = <&gpio>;
        interrupts = <25 2>;               /* GPIO25，2=下降沿觸發 */
        spi-max-frequency = <12000000>;    /* 12 MHz，可改低到 4MHz 除錯 */
        status = "okay";
    };
};

&spidev0 {
    status = "disabled";   /* 把 CS0 讓給 enc28j60，停用 spidev0 */
};

&gpio {
    eth1_pins: eth1_pins {
        brcm,pins = <25>;
        brcm,function = <0>;   /* INPUT */
        brcm,pull = <0>;       /* no pull */
    };
};
```

### 常見修改點

| 要改什麼 | 改哪裡 | 範例 |
|:---------|:-------|:-----|
| 換 CS 腳（CS1） | `reg = <1>` + `&spidev1` disabled | `reg = <1>;` |
| 換 INT 腳（GPIO24） | `interrupts = <24 2>` + `brcm,pins = <24>` | - |
| 降低 SPI 速度除錯 | `spi-max-frequency` | `<4000000>` = 4MHz |

### 編譯與套用 DTS

```bash
# 1. 編譯 DTS → DTB
dtc -@ -I dts -O dtb -o enc28j60-custom.dtbo enc28j60-overlay.dts

# 2. 複製到 overlays 目錄
sudo cp enc28j60-custom.dtbo /boot/firmware/overlays/

# 3. 在 /boot/firmware/config.txt 加入（或確認已有）
dtoverlay=enc28j60-custom

# 4. 重開機套用
sudo reboot
```

> **注意**：如果用的是系統內建的 `dtoverlay=enc28j60`，它會載入內建驅動。
> 自己的 driver 要用自己編譯的 dtbo，或確認 compatible 字串匹配。

---

## Build & 載入驅動

### Build

```bash
cd ~/GitHub/linux_kernel_driver/07_enc28j60_driver
make
```

### 載入（必須先 rmmod 內建驅動）

系統開機後內建的 `enc28j60` module 會自動 probe，需先卸載才能載入自己的：

```bash
# 卸載內建 driver（如果有的話）
sudo rmmod enc28j60 2>/dev/null

# 卸載自己舊版（如果有的話）
sudo rmmod enc28j60_driver 2>/dev/null

# 載入自己的 driver
sudo insmod enc28j60_driver.ko

# 看 kernel log
dmesg | tail -10
```

### 卸載

```bash
sudo rmmod enc28j60_driver
```

### 清除編譯產物

```bash
make clean
```

---

## 測試方式

### Step 1 測試 — probe 被呼叫

```bash
sudo insmod enc28j60_driver.ko
dmesg | tail -5
```

**預期輸出：**
```
enc28j60: probe — SPI bus=0 CS=0
```

---

### Step 2a 測試 — SPI 通訊 + EREVID

```bash
sudo rmmod enc28j60 2>/dev/null
sudo insmod enc28j60_driver.ko
dmesg | tail -6
```

**預期輸出：**
```
enc28j60: probe — SPI bus=0 CS=0
enc28j60: EREVID = 0x06
enc28j60: SPI 通訊正常，晶片版本 B6 ✓
```

**EREVID 異常判讀：**

| EREVID 值 | 代表什麼 | 解法 |
|:----------|:---------|:-----|
| `0x05` / `0x06` | 正常（B5 / B6 晶片）| - |
| `0xFF` | MISO 恆高，晶片未接或未供電 | 檢查接線、3.3V 電源 |
| `0x00` | MISO 恆低 | 檢查接線，或 bank 切換錯誤 |
| `0x40` | SPI 時序問題 | 降低 spi-max-frequency，或確認 SPI_MODE_0 |

---

### Step 2b 測試 ✅ — hw_init 完成

```bash
sudo rmmod enc28j60 2>/dev/null
sudo insmod enc28j60_driver.ko
dmesg | tail -5
```

**實際輸出：**
```
enc28j60: probe — SPI bus=0 CS=0
enc28j60: EREVID = 0x06
enc28j60: SPI 通訊正常，晶片版本 B6 ✓
enc28j60: hw_init 完成，MAC=02:42:ac:11:00:01，RXEN 已開啟
```

**代表什麼：**
- RX buffer 0x0000~0x19FF 設定完成，ERXRDPT=0x19FF（奇數，errata #14）
- TX buffer 起點 ETXST=0x1A00 設定完成
- ERXFCON = UCEN|CRCEN|BCEN（接收 unicast + broadcast，過濾壞 CRC）
- MACON1/3/4 MAC 子系統啟動，full duplex，auto pad + CRC
- MAC address 02:42:ac:11:00:01 寫入 MAADR1~6
- ECON1.RXEN = 1（BFS 原子設定），晶片開始接收封包

---

### Step 3 測試（待做）— net_device 出現

```bash
ip link show
# 應出現 eth1
```

---

### Step 4 測試（待做）— TX 路徑

```bash
sudo tcpdump -i eth1 -e arp
sudo arping -I eth1 192.168.50.1
# tcpdump 應看到 ARP 封包
```

---

### Step 5 測試（待做）— ping 通

```bash
sudo ip addr add 192.168.50.200/24 dev eth1
sudo ip link set eth1 up
ping 192.168.50.1
```

---

## 實作進度

- [x] **Step 1**：SPI driver 骨架 + probe/remove（`module_spi_driver` 一行註冊）
- [x] **Step 2a**：Register helpers + EREVID 驗證（SPI 通訊確認）
  - `spi_write_op()` — 寫操作基礎函式
  - `spi_read_op()` — 讀操作，MAC/MII 自動加 dummy byte
  - `enc28j60_set_bank()` — bank cache，只改差異 bit
  - `enc28j60_soft_reset()` — errata workaround udelay(2000)
- [x] **Step 2b**：`enc28j60_hw_init()` — RX/TX buffer、ERXFCON、MACON1/3/4、MAMXFL、IPG、MAC address、ECON1.RXEN
  - RX buffer 0x0000~0x19FF / TX buffer 0x1A00~0x1FFF
  - ERXFCON = UCEN|CRCEN|BCEN（unicast + broadcast，過濾壞 CRC）
  - MACON1 = MARXEN|TXPAUS|RXPAUS，MACON3 = auto pad/CRC/full duplex
  - MAMXFL = 1518，MABBIPG = 0x15，MAIPGL/H = 0x12/0x0C
  - MAC address 寫入 MAADR5/6/3/4/1/2（Microchip 特定 byte 順序）
  - ECON1.RXEN = 1（BFS 原子設定，晶片開始收封包）
  - 驗證：dmesg 顯示「hw_init 完成，RXEN 已開啟」✅
- [ ] **Step 3**：`net_device` 註冊 + open / stop
- [ ] **Step 4**：TX 路徑（sk_buff → SRAM → TXRTS）
- [ ] **Step 5**：IRQ + NAPI RX → ping 通

---

## Kernel 6.12 API 差異

這些是從舊版 code 移植時會踩到的坑：

| 舊 API | Kernel 6.12 正確寫法 | 說明 |
|:-------|:---------------------|:-----|
| `spi->master` | `spi->controller` | master 改名為 controller |
| `spi->chip_select` | `spi_get_chipselect(spi, 0)` | 改用 helper function 取 CS 編號 |
| `spi->master->bus_num` | `spi->controller->bus_num` | 同上，controller 改名 |

---

## 核心概念筆記

### Register 地址編碼（同官方 enc28j60_hw.h）

```
bit[7]    = SPRD_MASK  → 此暫存器是 MAC/MII，讀取需多一個 dummy byte
bit[6:5]  = bank       → 所在 bank（0~3），set_bank 用這個決定要不要切換
bit[4:0]  = addr       → 在 bank 內的地址
```

範例：
```c
#define EREVID  (0x12 | 0x60)        /* bank3 ETH register，無 dummy byte */
#define MACON1  (0x00 | 0x40 | 0x80) /* bank2 MAC register，需 dummy byte（SPRD_MASK=0x80）*/
```

### SPI Opcode 格式

```
bit[7:5] = opcode
bit[4:0] = register address（低 5 bit）
```

| Opcode | 值 | 說明 |
|:-------|:---|:-----|
| RCR（Read Control Register）| `0x00` | 讀暫存器 |
| WCR（Write Control Register）| `0x40` | 寫暫存器 |
| BFS（Bit Field Set）| `0x80` | 硬體直接 SET bit，省 read-modify-write |
| BFC（Bit Field Clear）| `0xA0` | 硬體直接 CLR bit |
| SRC（Soft Reset）| `0xFF` | 全晶片 reset |
| RBM（Read Buffer Memory）| `0x3A` | 讀 SRAM（RX 封包）|
| WBM（Write Buffer Memory）| `0x7A` | 寫 SRAM（TX 封包）|

### Soft Reset Errata（B7 silicon errata）

ENC28J60 B7 版本（`EREVID=0x06`）有 silicon bug：送出 SRC 後 CLKRDY bit 可能長時間不回 1，
永遠 polling 會卡死。官方做法：固定等 `udelay(2000)`（2ms），不 poll CLKRDY。

```c
static void enc28j60_soft_reset(struct enc28j60_priv *priv)
{
    spi_write_op(priv, ENC28J60_SOFT_RESET, 0, ENC28J60_SOFT_RESET);
    udelay(2000);       /* errata workaround：固定等 2ms */
    priv->bank = 0;     /* reset 後 chip 回 bank0 */
}
```

### Bank 切換細節

- ECON1[1:0]（BSEL1:BSEL0）控制目前 bank
- `priv->bank` cache 目前的值，只有真正需要切換時才發 SPI
- `priv->bank` **必須初始化為 0**，不能用 0xFF
  - 0xFF 的 bit[1:0] = 11 = bank3，set_bank 會誤以為已在正確 bank 而跳過切換
- All-bank registers（EIE / EIR / ESTAT / ECON2 / ECON1，地址 0x1B~0x1F）任何 bank 都能直接存取，不需要切換

### hw_init() 六步驟流程

```
hw_init(priv, mac_addr)
│
├─ ① 設定 RX buffer 範圍
│    ERXSTL/H   = 0x0000   RX 起點，必須偶數（errata #3）
│    ERXNDL/H   = 0x19FF   RX 終點（共 6656 bytes）
│    ERXRDPTL/H = 0x19FF   讀指標 = 終點，表示 buffer 是空的
│                           且必須是奇數（errata #14）
│    來源：datasheet Section 3.1.1
│
├─ ② 設定 TX buffer 起點
│    ETXSTL/H = 0x1A00     緊接 RX 之後，共 1536 bytes
│    來源：datasheet Section 7.1
│
├─ ③ 設定 RX filter（ERXFCON）
│    ERXFCON = UCEN | CRCEN | BCEN
│    UCEN = 0x80   只收目標 MAC 是我的封包（unicast）
│    CRCEN = 0x20  CRC 錯誤的封包直接丟棄
│    BCEN = 0x01   接收廣播（ARP request 靠這個進來）
│    ANDOR = 0     OR 邏輯：符合任一條件就收
│    來源：datasheet Section 8.0, Table 8-1
│
├─ ④ MAC 子系統設定
│    MACON1 = MARXEN | TXPAUS | RXPAUS
│      MARXEN  打開 MAC RX，沒這個 MAC 層不收封包
│      TXPAUS / RXPAUS  支援 pause frame（流量控制）
│
│    MACON3 = PADCFG0 | TXCRCEN | FRMLNEN | FULDPX
│      PADCFG0  短封包自動 pad 到 60 bytes
│      TXCRCEN  TX 自動附加 4-byte CRC，不需軟體計算
│      FRMLNEN  開啟 frame 長度合法性檢查
│      FULDPX   full duplex 模式
│
│    MACON4 = DEFER（0x40）
│      half duplex 時 medium busy 需延遲 TX，官方 driver 也設此 bit
│
│    MAMXFLL/H = 1518
│      最大合法 frame = 14（header）+ 1500（payload）+ 4（CRC）
│
│    MABBIPG = 0x15，MAIPGL = 0x12，MAIPGH = 0x0C
│      inter-packet gap，datasheet Section 6.5 推薦固定值
│    來源：datasheet Section 6.5 MAC Initialization Settings
│
├─ ⑤ 寫 MAC address
│    MAADR5=mac[0]  MAADR6=mac[1]
│    MAADR3=mac[2]  MAADR4=mac[3]
│    MAADR1=mac[4]  MAADR2=mac[5]
│    Microchip 特定 byte 順序，照官方 driver 對應即可
│    來源：datasheet Table 3-3, Section 6.5
│
└─ ⑥ 開啟 RX 總開關
     ECON1.RXEN = 1（BFS 指令原子設定）
     BFS opcode = 0x80|addr，送 [opcode][mask]
     只 SET 指定 bit，其他 bit 不動，避免 read-modify-write 競爭
     設完後晶片立即開始接收封包
     來源：datasheet Section 4.2.4, Section 7.2.1
```

### ERXRDPT 奇數 Errata

更新 RX 讀取指標（ERXRDPT）時，寫入值必須是奇數：

```c
/* 正確寫法 */
if (next_pkt == RXBUF_START)
    erxrdpt = RXBUF_END;      /* 0x17FF，奇數 ✓ */
else
    erxrdpt = next_pkt - 1;  /* next_pkt 一定是偶數，-1 變奇數 ✓ */
```

---

## 參考資料

| 資料 | 連結 |
|:-----|:-----|
| **官方 kernel driver（最重要）** | https://github.com/torvalds/linux/blob/master/drivers/net/ethernet/microchip/ |
| ENC28J60 Datasheet | https://ww1.microchip.com/downloads/en/devicedoc/39662e.pdf |
| Linux SPI subsystem | `include/linux/spi/spi.h` |
| sk_buff 結構 | `include/linux/skbuff.h` |
| net_device ops | `include/linux/netdevice.h` |
