#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/timer.h>  // 核心計時器
#include <linux/jiffies.h> // 時間單位轉換
#include <linux/kobject.h> // Sysfs 物件
#include <linux/string.h>
#include <linux/sysfs.h>

#define DRIVER_AUTHOR "Frank Huang"
#define DRIVER_DESC   "LED Blinking Driver with Sysfs Control"

// Raspberry Pi 5 GPIO (Same as before: GPIO 17 = 586)
#define LED_GPIO 586

// 全域變數
static struct timer_list my_timer; // 計時器結構
static int blink_period_ms = 1000; // 閃爍週期 (預設 1000ms)
static bool led_state = false;     // LED 當前狀態

// Sysfs 相關變數
static struct kobject *my_kobj;

// --- 計時器 Callback 函數 ---
// 當時間到時，核心會執行這個函數
void timer_callback(struct timer_list *t) {
    // 1. 切換 LED 狀態
    led_state = !led_state;
    gpio_set_value(LED_GPIO, led_state);

    // 2. 重新設定計時器 (讓它再次觸發，形成循環)
    // jiffies 是系統當前的 tick 數
    // msecs_to_jiffies 把毫秒轉成 tick
    mod_timer(&my_timer, jiffies + msecs_to_jiffies(blink_period_ms));
}

// --- Sysfs 讀取函數 (show) ---
// 當用戶 cat /sys/kernel/my_led/period_ms 時觸發
static ssize_t period_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
    return sprintf(buf, "%d\n", blink_period_ms);
}

// --- Sysfs 寫入函數 (store) ---
// 當用戶 echo 500 > /sys/kernel/my_led/period_ms 時觸發
static ssize_t period_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count) {
    int ret;
    int new_period;

    // 把字串轉成整數 (kstrtoint 是核心版的 atoi)
    ret = kstrtoint(buf, 10, &new_period);
    if (ret < 0) return ret;

    // 安全檢查：不能太快也不能太慢
    if (new_period < 10) new_period = 10;       // 最小 10ms
    if (new_period > 10000) new_period = 10000; // 最大 10秒

    blink_period_ms = new_period;
    printk(KERN_INFO "LED_BLINK: Period changed to %d ms\n", blink_period_ms);

    return count;
}

// 定義 Sysfs 屬性
// 建立一個名為 period_ms 的檔案，權限 0666 (可讀寫)
static struct kobj_attribute period_attr = __ATTR(period_ms, 0666, period_show, period_store);

// --- 初始化 ---
static int __init led_blink_init(void) {
    int ret;
    printk(KERN_INFO "LED_BLINK: Initializing\n");

    // 1. 申請 GPIO
    if (!gpio_is_valid(LED_GPIO)) return -ENODEV;
    if (gpio_request(LED_GPIO, "sysfs_blink_led")) return -EBUSY;
    gpio_direction_output(LED_GPIO, 0);

    // 2. 初始化計時器
    // 設定 callback 是 timer_callback
    timer_setup(&my_timer, timer_callback, 0);

    // 3. 啟動計時器 (現在時間 + 週期)
    ret = mod_timer(&my_timer, jiffies + msecs_to_jiffies(blink_period_ms));
    if (ret) printk(KERN_ERR "LED_BLINK: Error arming timer\n");

    // 4. 建立 Sysfs 介面 (/sys/kernel/my_led/)
    // kernel_kobj 指向 /sys/kernel
    my_kobj = kobject_create_and_add("my_led", kernel_kobj);
    if (!my_kobj) return -ENOMEM;

    // 建立屬性檔案 period_ms
    ret = sysfs_create_file(my_kobj, &period_attr.attr);
    if (ret) kobject_put(my_kobj);

    return 0;
}

// --- 卸載 ---
static void __exit led_blink_exit(void) {
    // 1. 刪除計時器 (非常重要！不然卸載後核心還會試圖呼叫你的函數 -> Crash)
    del_timer(&my_timer);

    // 2. 移除 Sysfs
    kobject_put(my_kobj);

    // 3. 釋放 GPIO
    gpio_set_value(LED_GPIO, 0);
    gpio_free(LED_GPIO);
    
    printk(KERN_INFO "LED_BLINK: Goodbye\n");
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);

module_init(led_blink_init);
module_exit(led_blink_exit);
