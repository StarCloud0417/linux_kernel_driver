# 02 - Character Device Driver (The Echo Device) 🗣️

這是一個基礎的字元驅動程式 (Character Driver) 範例。
它展示了如何註冊一個裝置，並實作 `open`, `read`, `write`, `release` 等核心介面。
這個裝置就像一個回音壁：你寫入什麼，讀取時就會得到什麼。

## 學習重點 (Key Concepts)

1.  **file_operations**: 這是驅動程式的靈魂結構，定義了 User Space 的系統呼叫 (System Call) 對應到 Kernel Space 的哪個函數。
2.  **register_chrdev**: 向核心申請一個「主設備號 (Major Number)」。
3.  **copy_to_user / copy_from_user**: 在核心空間 (Kernel Space) 與用戶空間 (User Space) 之間安全地搬運資料。**絕對不能使用 `memcpy`！**
4.  **mknod**: 手動建立裝置節點 (`/dev/xxx`) 來與驅動程式連結。

## 如何測試 (How to Test)

### 1. 編譯 (Build)
```bash
make
```

### 2. 載入模組 (Load)
```bash
sudo insmod et_driver.ko
```

### 3. 尋找主設備號 (Find Major Number)
核心會動態分配一個號碼，請檢查 Log：
```bash
dmesg | tail
```
你會看到類似這樣的訊息：
> ET: Registered correctly with major number **243**

### 4. 建立裝置節點 (Create Device Node)
使用剛剛找到的號碼 (假設是 243) 來建立檔案：
```bash
# sudo mknod /dev/[名稱] c [Major] [Minor]
sudo mknod /dev/et_device c 243 0

# 開放權限讓一般用戶也能讀寫
sudo chmod 666 /dev/et_device
```

### 5. 互動測試 (Interact)
現在 `/dev/et_device` 就是你的裝置了！

**寫入資料：**
```bash
echo "Hello Kernel!" > /dev/et_device
```

**讀取資料：**
```bash
cat /dev/et_device
# Output: Hello Kernel!
```

**查看核心運作紀錄：**
```bash
dmesg | tail
# 你會看到驅動程式印出的:
# ET: Received 14 characters from the user
# ET: Sent 14 characters to the user
```

### 6. 卸載與清理 (Clean Up)
```bash
sudo rm /dev/et_device
sudo rmmod et_driver
```
