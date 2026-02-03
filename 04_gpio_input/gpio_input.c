#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/interrupt.h> // 核心中斷處理介面

#define DRIVER_AUTHOR "Frank Huang"
#define DRIVER_DESC   "A simple GPIO Interrupt Driver (Button)"

// Raspberry Pi 5 Special Note:
// GPIO 27 (Physical Pin 13)
// From previous log: gpio-596 (GPIO27)
#define BUTTON_GPIO 596
#define DEVICE_NAME "my_button"

// 變數宣告
static int irq_number;      // 存放從 GPIO 轉換來的中斷號碼 (IRQ Number)
static int number_presses = 0;  // 記錄按下次數

// --- 中斷處理函式 (Interrupt Service Routine, ISR) ---
// 當按鈕被按下（觸發中斷）時，CPU 會暫停手邊工作，馬上跳來執行這個函式。
// 注意：ISR 必須執行得非常快，不能有 sleep 或耗時操作。
static irqreturn_t gpio_irq_handler(int irq, void *dev_id) {
    number_presses++;
    printk(KERN_INFO "GPIO_IRQ: Interrupt! (Button Pressed %d times)\n", number_presses);
    return IRQ_HANDLED; // 告訴核心：這個中斷我處理掉了
}

// --- 初始化 ---
static int __init gpio_irq_init(void) {
    int result = 0;
    printk(KERN_INFO "GPIO_IRQ: Initializing the Button LKM\n");

    // 1. 申請 GPIO
    if (!gpio_is_valid(BUTTON_GPIO)) {
        printk(KERN_INFO "GPIO_IRQ: Invalid GPIO\n");
        return -ENODEV;
    }

    result = gpio_request(BUTTON_GPIO, "sysfs_button");
    if (result) {
        printk(KERN_INFO "GPIO_IRQ: Failed to request GPIO %d\n", BUTTON_GPIO);
        return result;
    }

    // 2. 設定為輸入模式 (Input)
    gpio_direction_input(BUTTON_GPIO);

    // 3. 將 GPIO 號碼轉換為 IRQ 號碼
    // 在 Linux 裡，GPIO 和 IRQ 是兩套不同的編號系統。
    // 我們需要這一步才能向 CPU 註冊中斷。
    irq_number = gpio_to_irq(BUTTON_GPIO);
    printk(KERN_INFO "GPIO_IRQ: The button is mapped to IRQ: %d\n", irq_number);

    // 4. 申請中斷 (Request IRQ)
    // - irq_number: 中斷號
    // - gpio_irq_handler: 當中斷發生時要呼叫誰
    // - IRQF_TRIGGER_RISING: 指定觸發時機 (RISING = 電壓從 0 變 1 的瞬間)
    // - "my_button_handler": 出現在 /proc/interrupts 的名稱
    // - NULL: dev_id (這是一個傳給 handler 的參數，簡單範例用 NULL 即可)
    result = request_irq(irq_number,             
                         (irq_handler_t) gpio_irq_handler, 
                         IRQF_TRIGGER_RISING,   
                         "my_button_handler",    
                         NULL);

    if (result) {
        printk(KERN_INFO "GPIO_IRQ: Failed to request IRQ: %d\n", result);
        gpio_free(BUTTON_GPIO);
        return result;
    }

    printk(KERN_INFO "GPIO_IRQ: Module loaded successfully. Press the button!\n");
    return 0;
}

// --- 卸載 ---
static void __exit gpio_irq_exit(void) {
    // 1. 釋放中斷 (這很重要！不然下次按按鈕 CPU 會找不到 handler 而崩潰)
    free_irq(irq_number, NULL);
    
    // 2. 釋放 GPIO
    gpio_free(BUTTON_GPIO);
    
    printk(KERN_INFO "GPIO_IRQ: Goodbye from the LKM!\n");
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);

module_init(gpio_irq_init);
module_exit(gpio_irq_exit);
