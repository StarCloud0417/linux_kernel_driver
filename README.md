# Linux Kernel Driver Learning Journey 🚀

這是我 (Frank Huang) 從零開始學習 Linux 核心驅動程式開發的學習筆記與實作專案。
目標是從最基礎的 Hello World 開始，一步步深入到真正的硬體控制，最終達到精通 Linux Driver 開發的境界。

## 專案結構 (Project Structure)

每個目錄代表一個學習階段或一個特定的主題：

| 章節 | 專案名稱 | 描述 |
| :--- | :--- | :--- |
| 01 | **hello_world** | 驅動程式的起點。學習模組架構 (`init`, `exit`)、`printk` 與 `Makefile` 編譯流程。 |

## 環境需求 (Environment)

- **OS**: Linux (Raspberry Pi OS / Ubuntu / Debian)
- **Kernel**: 6.x (Tested on Raspberry Pi 5)
- **Tools**: `gcc`, `make`, `kernel-headers`

## 如何執行 (How to Run)

以 `01_hello_world` 為例：

1. **進入目錄**
   ```bash
   cd 01_hello_world
   ```

2. **編譯模組**
   ```bash
   make
   ```
   成功後會產生 `hello.ko` (Kernel Object) 檔案。

3. **載入模組 (Load Module)**
   ```bash
   sudo insmod hello.ko
   ```

4. **查看核心日誌 (Check Logs)**
   核心訊息不會直接顯示在終端機，需透過 `dmesg` 查看：
   ```bash
   sudo dmesg | tail
   ```
   應可看到輸出： `Hello, Kernel! I am Frank's driver.`

5. **卸載模組 (Unload Module)**
   ```bash
   sudo rmmod hello
   ```
   查看卸載訊息：
   ```bash
   sudo dmesg | tail
   ```

## 學習筆記 (Notes)

- **Kernel Space vs User Space**: 驅動程式運行在核心空間，權力無限但也伴隨高風險（可能導致系統崩潰）。
- **printk**: 核心層級的 `printf`，具有不同的 Log Level (如 `KERN_INFO`)。
- **Makefile**: 必須使用 Kernel Build System (Kbuild) 的規範來編譯 `.ko` 檔。

---
*Created by Frank Huang with the assistance of Liuli (AI Assistant).*
