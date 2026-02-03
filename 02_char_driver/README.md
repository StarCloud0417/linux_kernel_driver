# 02 - Character Device Driver (The Echo Device) 🗣️

這是一個基礎的字元驅動程式 (Character Driver) 範例。
它展示了如何註冊一個裝置，並實作 `open`, `read`, `write`, `release` 等核心介面。

## 為什麼要這樣寫？ (The "Why")

(略，保留原本的說明)

## 程式運作流程圖 (Execution Flow)

```mermaid
sequenceDiagram
    participant User as User App (User Space)
    participant VFS as VFS (Virtual File System)
    participant Driver as ET Driver (Kernel Space)
    participant HW as Kernel Buffer

    Note over User, Driver: 1. 載入與初始化
    User->>Driver: insmod et_driver.ko
    Driver->>Driver: char_driver_init()
    Driver->>VFS: register_chrdev() (申請 Major Number)

    Note over User, Driver: 2. 建立節點 (mknod)
    User->>VFS: mknod /dev/et_device
    
    Note over User, Driver: 3. 寫入資料 (Write)
    User->>VFS: echo "Hello" > /dev/et_device
    VFS->>Driver: dev_write("Hello")
    Driver->>HW: copy_from_user()
    HW-->>Driver: (資料存入 kernel_buffer)
    Driver-->>VFS: return bytes_written
    VFS-->>User: (寫入成功)

    Note over User, Driver: 4. 讀取資料 (Read)
    User->>VFS: cat /dev/et_device
    VFS->>Driver: dev_read()
    HW-->>Driver: (從 kernel_buffer 取出資料)
    Driver->>User: copy_to_user("Hello")
    Driver-->>VFS: return bytes_read
    VFS-->>User: 顯示 "Hello"

    Note over User, Driver: 5. 卸載
    User->>Driver: rmmod et_driver
    Driver->>VFS: unregister_chrdev()
```

### 流程說明：
1.  **註冊 (Registration)**：驅動程式啟動時，向核心（VFS）掛號，說：「我是驅動程式，我負責 Major Number 243，如果有對應的操作請轉交給我。」
2.  **寫入 (Write Path)**：
    *   用戶 `echo` -> 觸發 `sys_write` 系統呼叫。
    *   核心看到是針對我們的裝置，於是轉呼叫 `dev_write`。
    *   我們用 `copy_from_user` 把資料從用戶搬進核心的暫存區 (`kernel_buffer`)。
3.  **讀取 (Read Path)**：
    *   用戶 `cat` -> 觸發 `sys_read` 系統呼叫。
    *   核心轉呼叫 `dev_read`。
    *   我們用 `copy_to_user` 把暫存區的資料搬回給用戶。

## 如何測試 (How to Test)
(略，同前版)
