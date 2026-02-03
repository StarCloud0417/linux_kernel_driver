#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>           // 檔案系統相關 (file_operations, register_chrdev)
#include <linux/uaccess.h>      // copy_to_user, copy_from_user

#define DEVICE_NAME "et_device" // 裝置名稱，會出現在 /proc/devices
#define BUFFER_SIZE 1024        // 我們的內部緩衝區大小

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Frank Huang");
MODULE_DESCRIPTION("A Simple Character Device Driver (Echo Device)");

// 全域變數
static int major_number;                  // 核心分配給我們的主編號
static char kernel_buffer[BUFFER_SIZE];   // 核心空間的緩衝區
static short size_of_message;             // 目前緩衝區內資料的長度

// 前置宣告 (Prototypes)
// 這些是我們即將實作的函數，對應到 User Space 的系統呼叫
static int dev_open(struct inode *, struct file *);
static int dev_release(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char __user *, size_t, loff_t *);
static ssize_t dev_write(struct file *, const char __user *, size_t, loff_t *);

// 核心結構：告訴核心，當有人對這個裝置做動作時，要呼叫哪個函數
// 這是驅動程式的靈魂！
static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = dev_open,
    .read = dev_read,
    .write = dev_write,
    .release = dev_release,
};

// --- 初始化函數 ---
static int __init char_driver_init(void) {
    printk(KERN_INFO "ET: Initializing the EtChar LKM\n");

    // 1. 動態申請一個 Major Number
    // 第一個參數 0 表示讓核心自動分配
    major_number = register_chrdev(0, DEVICE_NAME, &fops);

    if (major_number < 0) {
        printk(KERN_ALERT "ET: Failed to register a major number\n");
        return major_number;
    }

    printk(KERN_INFO "ET: Registered correctly with major number %d\n", major_number);
    printk(KERN_INFO "ET: Create a device node manually with: sudo mknod /dev/%s c %d 0\n", DEVICE_NAME, major_number);
    return 0;
}

// --- 卸載函數 ---
static void __exit char_driver_exit(void) {
    // 註銷裝置，把號碼牌還給核心
    unregister_chrdev(major_number, DEVICE_NAME);
    printk(KERN_INFO "ET: Goodbye from the LKM!\n");
}

// --- 檔案操作實作 ---

// 當用戶 open("/dev/et_device") 時
static int dev_open(struct inode *inodep, struct file *filep) {
    printk(KERN_INFO "ET: Device has been opened\n");
    return 0;
}

// 當用戶 read(fd, buffer, len) 時
static ssize_t dev_read(struct file *filep, char __user *buffer, size_t len, loff_t *offset) {
    int error_count = 0;

    // 如果 offset 已經超過資料長度，代表讀完了，回傳 0 (EOF)
    if (*offset >= size_of_message) {
        return 0;
    }

    // 計算要讀多少 byte (避免讀超過 buffer)
    if (len > size_of_message - *offset) {
        len = size_of_message - *offset;
    }

    // copy_to_user(目標(User), 來源(Kernel), 長度)
    // 這是一個「黑魔法」函數，它能跨越 User/Kernel 邊界
    error_count = copy_to_user(buffer, kernel_buffer + *offset, len);

    if (error_count == 0) {
        printk(KERN_INFO "ET: Sent %zu characters to the user\n", len);
        *offset += len; // 更新讀取指標，下次從這裡繼續讀
        return len;     // 回傳實際讀到的長度
    } else {
        printk(KERN_INFO "ET: Failed to send %d characters to the user\n", error_count);
        return -EFAULT; // 回傳錯誤碼
    }
}

// 當用戶 write(fd, buffer, len) 時
static ssize_t dev_write(struct file *filep, const char __user *buffer, size_t len, loff_t *offset) {
    if (len > BUFFER_SIZE - 1) {
        len = BUFFER_SIZE - 1; // 避免緩衝區溢位
    }

    // copy_from_user(目標(Kernel), 來源(User), 長度)
    // 把用戶傳來的資料搬進我們的 kernel_buffer
    if (copy_from_user(kernel_buffer, buffer, len) != 0) {
        return -EFAULT;
    }

    kernel_buffer[len] = '\0'; // 補上字串結尾符號
    size_of_message = len;     // 記錄長度
    
    printk(KERN_INFO "ET: Received %zu characters from the user\n", len);
    return len; // 回傳實際寫入的長度
}

// 當用戶 close(fd) 時
static int dev_release(struct inode *inodep, struct file *filep) {
    printk(KERN_INFO "ET: Device successfully closed\n");
    return 0;
}

module_init(char_driver_init);
module_exit(char_driver_exit);
