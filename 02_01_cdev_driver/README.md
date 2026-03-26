# 02_01_cdev_driver (Modern Character Device Driver)

這是一個現代化的 Linux 字元裝置驅動程式 (Character Device Driver) 範例。
此專案作為 `02_char_driver` 的進階版本，展示了自 Linux Kernel 2.6 版之後的標準開發流程。

## 🌟 本專案重點升級

與傳統的 `register_chrdev` 相比，本專案引入了以下現代化核心機制：

1. **動態分配設備號 (`alloc_chrdev_region`)**
   - 不再霸佔 256 個 Minor Number。
   - 讓 Kernel 自動發配一組安全的 Major Number 給我們，避免與其他設備衝突。

2. **使用 cdev 結構 (`cdev_init`, `cdev_add`)**
   - 將設備的註冊分為「申請設備號」與「添加設備實體」兩個步驟，結構更清晰。

3. **自動建立設備節點 (`class_create`, `device_create`)**
   - 結合 `udev` 機制。
   - 模組掛載 (`insmod`) 時，自動在 `/dev/` 目錄下產生 `cdev_device`。
   - 模組卸載 (`rmmod`) 時，自動回收並刪除節點，**完全免除手動執行 `mknod` 的困擾！**

## 📂 檔案說明

- `cdev_driver.c`: 驅動程式核心原始碼。
- `test_cdev_app.c`: 使用者空間 (User Space) 的測試應用程式。
- `Makefile`: 用於編譯 Kernel Module 與測試程式。

## 🚀 測試步驟 (Test Drive)

### 1. 編譯專案
```bash
make
```

### 2. 掛載驅動程式 (全自動建立節點)
```bash
sudo insmod cdev_driver.ko
```
*您可以透過 `dmesg | tail` 確認掛載訊息，並用 `ls -l /dev/cdev_device` 確認系統已自動建立節點。*

### 3. 設定權限並執行測試程式
```bash
# 賦予讀寫權限，讓一般使用者也能測試
sudo chmod 666 /dev/cdev_device

# 執行測試程式
./test_cdev_app
```

### 4. 卸載驅動程式 (全自動清除)
```bash
sudo rmmod cdev_driver
```
*卸載後，`/dev/cdev_device` 會自動被系統移除。最後可執行 `make clean` 清除編譯產生的檔案。*