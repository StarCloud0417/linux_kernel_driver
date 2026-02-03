# 03 - GPIO Output Driver (LED Control) 💡

這堂課我們進入實戰：控制硬體。
我們將寫一個驅動程式來控制 Raspberry Pi 5 的 GPIO 腳位，實現點燈功能。

## Raspberry Pi 5 特別注意事項 (RP1 Chip) ⚠️
(略，保留原本的說明)

## 程式運作流程圖 (Execution Flow)

```mermaid
graph TD
    subgraph Initialization [初始化階段 (insmod)]
        Init[gpio_driver_init] --> Check[gpio_is_valid?]
        Check -->|Yes| Request[gpio_request]
        Request -->|Success| Dir[gpio_direction_output = 0]
        Dir --> Reg[register_chrdev]
        Reg -->|Major Num| Done[等待呼叫]
    end

    subgraph Runtime [執行階段 (echo 1 > /dev/my_led)]
        User[User Input: "1"] -->|System Call| Kernel[sys_write]
        Kernel --> DriverWrite[dev_write]
        DriverWrite --> Copy[copy_from_user]
        Copy --> Logic{判斷輸入值}
        Logic -->|'1'| On[gpio_set_value = 1]
        Logic -->|'0'| Off[gpio_set_value = 0]
        On --> HW((LED 亮))
        Off --> HW((LED 滅))
    end

    subgraph Cleanup [卸載階段 (rmmod)]
        Exit[gpio_driver_exit] --> Reset[gpio_set_value = 0]
        Reset --> Free[gpio_free]
        Free --> Unreg[unregister_chrdev]
    end
```

### 流程說明：
1.  **資源申請 (Request)**：
    *   這一步最關鍵。我們必須先用 `gpio_request` 佔領這個腳位。
    *   如果失敗（例如腳位已被佔用），初始化就會直接中止，避免後續錯誤。
2.  **硬體設定 (Direction)**：
    *   GPIO 腳位可以是輸入（讀電壓）也可以是輸出（給電壓）。
    *   `gpio_direction_output` 強制將腳位設為輸出模式，並預設給低電位 (0)，確保 LED 一開始是滅的。
3.  **控制邏輯 (Control Logic)**：
    *   當用戶寫入字元時，我們只看第一個字元。
    *   如果是 '1'，呼叫 `gpio_set_value(..., 1)` -> 晶片輸出 3.3V -> LED 亮。
    *   如果是 '0'，呼叫 `gpio_set_value(..., 0)` -> 晶片輸出 0V -> LED 滅。
4.  **資源釋放 (Free)**：
    *   卸載時，務必 `gpio_free`，否則下次載入時會因為「資源忙碌」而失敗。

## 如何測試 (How to Test)
(略，同前版)
