# Linux Kernel Driver Learning Journey 🚀

這是我 (Frank Huang) 從零開始學習 Linux 核心驅動程式開發的學習筆記與實作專案。
目標是從最基礎的 Hello World 開始，一步步深入到真正的硬體控制，最終達到精通 Linux Driver 開發的境界。

## 專案結構 (Project Structure)

每個目錄代表一個學習階段或一個特定的主題：

| 章節 | 專案名稱 | 描述 |
| :--- | :--- | :--- |
| 01 | **[hello_world](./01_hello_world)** | 驅動程式的起點。學習模組架構 (`init`, `exit`)、`printk` 與 `Makefile` 編譯流程。 |
| 02 | **[char_driver](./02_char_driver)** | 傳統字元驅動程式。實作一個「迴音蟲」裝置，學習 `read`/`write`、`copy_to_user` 以及舊版 `register_chrdev`。 |
| 02.1 | **[cdev_driver](./02_01_cdev_driver)** | **[New]** 現代化字元驅動程式。學習 `alloc_chrdev_region` 動態分配設備號，並透過 `udev` (`class_create`, `device_create`) 實現全自動建立 `/dev` 節點。 |
| 03 | **[gpio_driver](./03_gpio_driver)** | 實體硬體控制 (Output)。透過寫入 `/dev` 檔案來控制 LED 亮滅，學習 GPIO 資源管理。 |
| 04 | **[gpio_input](./04_gpio_input)** | 實體硬體讀取 (Input)。使用中斷 (`request_irq`) 偵測按鈕訊號，取代傳統的 Polling。 |
| 05 | **[led_blinking](./05_led_blinking)** | 計時器與 Sysfs。實作自動閃爍 LED，並透過 `/sys/` 動態調整頻率。 |
| 06 | **[mutex_locking](./06_mutex_locking)** | 並發控制。使用 Mutex 鎖解決 Race Condition 問題，確保多執行緒下的資料安全。 |

## 環境需求 (Environment)

- **OS**: Linux (Raspberry Pi OS / Ubuntu / Debian)
- **Kernel**: 6.x (Tested on Raspberry Pi 5)
- **Tools**: `gcc`, `make`, `kernel-headers`

## 常用指令 Cheat Sheet

```bash
# 編譯
make

# 載入模組
sudo insmod my_module.ko

# 查看模組清單
lsmod | grep my_module

# 查看核心日誌 (Debug用)
sudo dmesg -w

# 卸載模組
sudo rmmod my_module
```

---
*Created by Frank Huang with the assistance of Liuli (AI Assistant).*