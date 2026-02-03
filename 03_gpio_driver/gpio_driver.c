#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/gpio.h>         // 舊版 GPIO 介面 (簡單易懂，適合教學)
#include <linux/fs.h>
#include <linux/uaccess.h>

#define DRIVER_AUTHOR "Frank Huang"
#define DRIVER_DESC   "A simple GPIO driver for LED control"

// 設定我們要控制的 GPIO Pin 腳號碼
// 在 Raspberry Pi 上，GPIO 21 (Pin 40) 通常是安全的測試點
// 請確認您的 LED 接在 GPIO 21 上 (實體腳位 40)
#define LED_GPIO 21
#define DEVICE_NAME "my_led"

static int major_number;

// --- 檔案操作函數 (Prototypes) ---
static int dev_open(struct inode *, struct file *);
static int dev_release(struct inode *, struct file *);
static ssize_t dev_write(struct file *, const char __user *, size_t, loff_t *);

// --- 檔案操作結構 ---
static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = dev_open,
    .write = dev_write,  // 我們只需要寫入 (控制亮滅)
    .release = dev_release,
};

// --- 初始化 (Module Init) ---
static int __init gpio_driver_init(void) {
    int result = 0;

    printk(KERN_INFO "GPIO_DRIVER: Initializing the GPIO LKM\n");

    // 1. 申請 GPIO 資源
    // 告訴核心：「這個號碼 (21) 我要用，別人不能搶。」
    if (!gpio_is_valid(LED_GPIO)) {
        printk(KERN_INFO "GPIO_DRIVER: Invalid GPIO\n");
        return -ENODEV;
    }

    // 申請並給予標籤 "sysfs_led"
    result = gpio_request(LED_GPIO, "sysfs_led");
    if (result < 0) {
        printk(KERN_ALERT "GPIO_DRIVER: Failed to request GPIO %d\n", LED_GPIO);
        return result;
    }

    // 2. 設定為輸出模式 (Output)，且預設為 0 (滅)
    gpio_direction_output(LED_GPIO, 0);

    // 3. 註冊字元裝置 (跟上一課一樣)
    major_number = register_chrdev(0, DEVICE_NAME, &fops);
    if (major_number < 0) {
        printk(KERN_ALERT "GPIO_DRIVER: Failed to register a major number\n");
        // 失敗時記得釋放 GPIO
        gpio_free(LED_GPIO);
        return major_number;
    }

    printk(KERN_INFO "GPIO_DRIVER: Registered correctly with major number %d\n", major_number);
    return 0;
}

// --- 卸載 (Module Exit) ---
static void __exit gpio_driver_exit(void) {
    // 1. 熄滅 LED (良好的習慣：離開時恢復原狀)
    gpio_set_value(LED_GPIO, 0);

    // 2. 釋放 GPIO 資源
    gpio_free(LED_GPIO);

    // 3. 註銷裝置
    unregister_chrdev(major_number, DEVICE_NAME);
    
    printk(KERN_INFO "GPIO_DRIVER: Goodbye from the LKM!\n");
}

// --- 實作 write 函數 ---
// 當用戶執行 `echo 1 > /dev/my_led` 時觸發
static ssize_t dev_write(struct file *filep, const char __user *buffer, size_t len, loff_t *offset) {
    char value;

    // 我們只需要讀取 1 個字元 (0 或 1)
    if (copy_from_user(&value, buffer, 1) != 0) {
        return -EFAULT;
    }

    if (value == '1') {
        gpio_set_value(LED_GPIO, 1); // 亮燈
        printk(KERN_INFO "GPIO_DRIVER: LED ON\n");
    } else if (value == '0') {
        gpio_set_value(LED_GPIO, 0); // 滅燈
        printk(KERN_INFO "GPIO_DRIVER: LED OFF\n");
    } else {
        printk(KERN_INFO "GPIO_DRIVER: Unknown command\n");
    }

    return len;
}

static int dev_open(struct inode *inodep, struct file *filep) {
    printk(KERN_INFO "GPIO_DRIVER: Device opened\n");
    return 0;
}

static int dev_release(struct inode *inodep, struct file *filep) {
    printk(KERN_INFO "GPIO_DRIVER: Device closed\n");
    return 0;
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);

module_init(gpio_driver_init);
module_exit(gpio_driver_exit);
