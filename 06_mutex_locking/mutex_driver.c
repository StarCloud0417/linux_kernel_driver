#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/mutex.h> // 互斥鎖定義
#include <linux/delay.h> // mdelay

#define DEVICE_NAME "race_car"
#define DRIVER_AUTHOR "Frank Huang"
#define DRIVER_DESC   "A driver demonstrating Race Condition and Mutex"

// 全域變數
static int major_number;
static int shared_counter = 0; // 我們要保護的共享資源
static struct mutex my_mutex;  // 定義一把鎖

// 前置宣告
static int dev_open(struct inode *, struct file *);
static int dev_release(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char __user *, size_t, loff_t *);
static ssize_t dev_write(struct file *, const char __user *, size_t, loff_t *);

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = dev_open,
    .read = dev_read,
    .write = dev_write,
    .release = dev_release,
};

// --- 初始化 ---
static int __init mutex_driver_init(void) {
    printk(KERN_INFO "MUTEX_DEMO: Initializing...\n");

    // 1. 初始化互斥鎖 (非常重要！一定要做)
    mutex_init(&my_mutex);

    // 2. 註冊裝置
    major_number = register_chrdev(0, DEVICE_NAME, &fops);
    if (major_number < 0) {
        printk(KERN_ALERT "MUTEX_DEMO: Failed to register a major number\n");
        return major_number;
    }

    printk(KERN_INFO "MUTEX_DEMO: Registered correctly with major number %d\n", major_number);
    return 0;
}

// --- 卸載 ---
static void __exit mutex_driver_exit(void) {
    // 1. 銷毀互斥鎖 (雖然 mutex_destroy 在目前核心通常是空的，但為了好習慣還是加上)
    mutex_destroy(&my_mutex);
    
    // 2. 註銷裝置
    unregister_chrdev(major_number, DEVICE_NAME);
    printk(KERN_INFO "MUTEX_DEMO: Goodbye\n");
}

// --- 讀取函數 ---
// 讀取目前的計數器值
static ssize_t dev_read(struct file *filep, char __user *buffer, size_t len, loff_t *offset) {
    char msg[64];
    int msg_len;
    int error_count;

    // 加鎖讀取 (雖然讀取 int 通常是原子操作，但為了示範完整性)
    mutex_lock(&my_mutex);
    int current_val = shared_counter;
    mutex_unlock(&my_mutex);

    msg_len = sprintf(msg, "Counter: %d\n", current_val);
    
    if (*offset > 0) return 0; // EOF

    error_count = copy_to_user(buffer, msg, msg_len);
    if (error_count == 0) {
        *offset += msg_len;
        return msg_len;
    } else {
        return -EFAULT;
    }
}

// --- 寫入函數 (關鍵！) ---
// 用戶寫入任何東西都會觸發計數器 +1
static ssize_t dev_write(struct file *filep, const char __user *buffer, size_t len, loff_t *offset) {
    
    printk(KERN_INFO "MUTEX_DEMO: Waiting for lock...\n");

    // 1. 獲取鎖 (Lock)
    // 如果別的執行緒已經拿走了鎖，程式會在這裡「卡住」(Sleep)，直到鎖被釋放。
    mutex_lock(&my_mutex);
    
    printk(KERN_INFO "MUTEX_DEMO: Lock acquired! Working...\n");

    // 2. 臨界區 (Critical Section)
    // 這裡模擬一個耗時的操作：讀取 -> 計算 -> 寫回
    // 如果沒有鎖，兩個執行緒同時讀到 0，同時 +1，最後結果是 1 (但應該是 2)
    int temp = shared_counter;
    
    // 模擬計算耗時 100ms
    // 在這 100ms 內，如果有別人也進來，就會發生悲劇
    mdelay(100); 
    
    temp++;
    shared_counter = temp;

    printk(KERN_INFO "MUTEX_DEMO: Updated counter to %d\n", shared_counter);

    // 3. 釋放鎖 (Unlock)
    // 讓下一個排隊的人進來
    mutex_unlock(&my_mutex);

    return len;
}

static int dev_open(struct inode *inodep, struct file *filep) {
    return 0;
}

static int dev_release(struct inode *inodep, struct file *filep) {
    return 0;
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);

module_init(mutex_driver_init);
module_exit(mutex_driver_exit);
