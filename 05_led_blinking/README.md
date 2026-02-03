# 05 - LED Blinking Driver (Timers & Sysfs) ⏳

這堂課我們讓 LED 「活起來」。
不再需要手動一直輸入 `echo 1`，驅動程式會自己計時並切換開關。
同時，我們引入了現代 Linux 驅動控制參數的標準介面：**Sysfs**。

## 學習重點 (Key Concepts)

1.  **Kernel Timer**: 核心的鬧鐘機制。
    *   `timer_setup`: 設定鬧鐘響了要執行哪個函數 (`callback`)。
    *   `mod_timer`: 設定鬧鐘多久後響。
    *   `del_timer`: 關閉鬧鐘（卸載時必做！）。
2.  **Jiffies**: 核心的時間滴答聲。我們用 `msecs_to_jiffies(1000)` 將人類的 1000ms 轉換為核心的滴答數。
3.  **Sysfs**: 在 `/sys/kernel/` 下建立目錄與檔案，讓用戶可以即時修改驅動程式的參數（如閃爍速度）。

## 程式運作流程圖 (Execution Flow)

```mermaid
graph TD
    %% Initialization
    subgraph Init ["1. 初始化 (insmod)"]
        direction TB
        A[led_blink_init] --> B[申請 GPIO]
        B --> C[timer_setup (設定 callback)]
        C --> D[mod_timer (啟動第一次計時)]
        D --> E[kobject_create (建立 /sys/kernel/my_led)]
        E --> F[sysfs_create_file (建立 period_ms)]
    end

    %% Timer Loop
    subgraph TimerLoop ["2. 自動循環 (Background)"]
        direction TB
        T[時間到!] -->|Trigger| CB[timer_callback]
        CB --> Toggle{切換狀態}
        Toggle -->|On| L1[LED 亮]
        Toggle -->|Off| L0[LED 滅]
        L1 --> Reset[mod_timer (預約下一次)]
        L0 --> Reset
        Reset --> T
    end

    %% Sysfs Interaction
    subgraph Sysfs ["3. 用戶控制 (Sysfs)"]
        direction TB
        UserWrite[echo 200 > period_ms] -->|store| Handler[period_store]
        Handler --> Update[更新 blink_period_ms]
        Update --> Next[下次 callback 生效]
    end

    %% Cleanup
    subgraph Exit ["4. 卸載 (rmmod)"]
        direction TB
        X[led_blink_exit] --> Y[del_timer (刪除計時器)]
        Y --> Z[kobject_put (移除 Sysfs)]
        Z --> G[gpio_free]
    end
```

## 如何測試 (How to Test)

1.  **編譯與載入**
    ```bash
    make
    sudo insmod led_blink.ko
    ```
    *(載入後，您的 LED 應該會開始每秒閃爍一次)*

2.  **查看 Sysfs 介面**
    ```bash
    ls -l /sys/kernel/my_led/
    # 應該會看到一個檔案: period_ms
    ```

3.  **讀取目前速度**
    ```bash
    cat /sys/kernel/my_led/period_ms
    # Output: 1000
    ```

4.  **改變速度 (變快！)**
    讓它變成 0.1 秒閃一次：
    ```bash
    sudo sh -c 'echo 100 > /sys/kernel/my_led/period_ms'
    ```
    *(注意：因為權限問題，直接用 `sudo echo` 可能會失敗，建議用 `sudo sh -c` 或先切換成 root)*

5.  **改變速度 (變慢～)**
    讓它變成 2 秒閃一次：
    ```bash
    sudo sh -c 'echo 2000 > /sys/kernel/my_led/period_ms'
    ```

6.  **卸載**
    ```bash
    sudo rmmod led_blink
    ```
    *(LED 應該會熄滅停止)*
