# 03 - GPIO Output Driver (LED Control) 💡

這堂課我們進入實戰：控制硬體。
我們將寫一個驅動程式來控制 Raspberry Pi 的 GPIO 腳位，實現點燈功能。

## 學習重點 (Key Concepts)

1.  **GPIO API**: 學習核心提供的 `gpio_request`, `gpio_direction_output`, `gpio_set_value` 等函數。
2.  **Resource Management**: 資源（如 GPIO Pin）是有限的，必須先「申請」才能用，用完必須「釋放」(`gpio_free`)。
3.  **Hardware Control**: 透過 `write` 系統呼叫來觸發硬體動作。

## 硬體接線 (Hardware Wiring)

請準備：
- 一顆 LED
- 一個電阻 (220Ω 或 330Ω)
- 麵包板與杜邦線

接線方式：
- **LED 正極 (長腳)** -> 接到 **GPIO 21** (實體 Pin 40)
- **LED 負極 (短腳)** -> 串聯電阻 -> 接到 **GND** (實體 Pin 39)

*(注意：如果您使用不同的 GPIO，請修改程式碼中的 `#define LED_GPIO`)*

## 如何測試 (How to Test)

### 1. 編譯與載入
```bash
make
sudo insmod gpio_driver.ko
```

### 2. 建立裝置節點
先查看 Major Number：
```bash
dmesg | tail
# 假設看到: Registered correctly with major number 244
```

建立節點：
```bash
sudo mknod /dev/my_led c 244 0
sudo chmod 666 /dev/my_led
```

### 3. 控制 LED
現在，您可以用文字來控制燈光了！

**開燈：**
```bash
echo 1 > /dev/my_led
```

**關燈：**
```bash
echo 0 > /dev/my_led
```

### 4. 故障排除
如果在載入時出現 `Device or resource busy`，表示該 GPIO 腳位正被其他程式 (如 Python 腳本或 gpiod) 佔用。請先關閉該程式，或重開機。

### 5. 清理
```bash
sudo rm /dev/my_led
sudo rmmod gpio_driver
```
