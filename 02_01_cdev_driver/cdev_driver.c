#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>           // 檔案系統相關 (alloc_chrdev_region)
#include <linux/cdev.h>         // cdev_init, cdev_add, cdev_del
#include <linux/device.h>       // class_create, device_create, device_destroy, class_destroy
#include <linux/uaccess.h>      // copy_to_user, copy_from_user
#include <linux/err.h>          // IS_ERR, PTR_ERR

#define DEVICE_NAME "cdev_device" // 裝置名稱 (將會出現在 /dev/ 底下)
#define CLASS_NAME  "et_class"    // 類別名稱 (將會出現在 /sys/class/ 底下)
#define BUFFER_SIZE 1024          // 內部緩衝區大小

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Frank Huang");
MODULE_DESCRIPTION("A Modern Character Device Driver with Auto Device Creation");

// 全域變數
static dev_t dev_num;                     // 存放動態分配到的 Major 和 Minor Number
static struct cdev et_cdev;               // 字元設備結構體
static struct class *dev_class = NULL;    // 設備類別指標 (用於自動建立節點)
static struct device *dev_device = NULL;  // 設備實體指標 (用於自動建立節點)
static int major_number;                  
static char kernel_buffer[BUFFER_SIZE];   
static short size_of_message;             

// 前置宣告 (Prototypes)
static int dev_open(struct inode *, struct file *);
static int dev_release(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char __user *, size_t, loff_t *);
static ssize_t dev_write(struct file *, const char __user *, size_t, loff_t *);

// 核心結構：綁定檔案操作
static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = dev_open,
    .read = dev_read,
    .write = dev_write,
    .release = dev_release,
    .llseek = default_llseek,
};

// --- 初始化函數 ---
static int __init cdev_driver_init(void) {
    int ret;
    printk(KERN_INFO "CDEV: Initializing the Modern Char LKM with Auto Mknod\n");

    // 1. 動態申請設備號 (Major + Minor)
    ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        printk(KERN_ALERT "CDEV: Failed to allocate chrdev region\n");
        return ret;
    }
    major_number = MAJOR(dev_num);
    printk(KERN_INFO "CDEV: Allocated successfully with major number %d\n", major_number);

    // 2. 初始化 cdev 結構並綁定 fops
    cdev_init(&et_cdev, &fops);
    et_cdev.owner = THIS_MODULE;

    // 3. 將 cdev 註冊進系統
    ret = cdev_add(&et_cdev, dev_num, 1);
    if (ret < 0) {
        unregister_chrdev_region(dev_num, 1);
        printk(KERN_ALERT "CDEV: Failed to add cdev to the system\n");
        return ret;
    }

    // 4. 自動建立 Device Node (udev 機制)
    // 4.1 建立 Class (/sys/class/et_class)
    dev_class = class_create(CLASS_NAME);
    if (IS_ERR(dev_class)) {
        printk(KERN_ALERT "CDEV: Failed to register device class\n");
        ret = PTR_ERR(dev_class);
        goto class_error;
    }

    // 4.2 建立 Device (/dev/cdev_device)
    // 參數：(所屬類別, 父設備, 設備號, 私有資料, 設備名稱)
    dev_device = device_create(dev_class, NULL, dev_num, NULL, DEVICE_NAME);
    if (IS_ERR(dev_device)) {
        printk(KERN_ALERT "CDEV: Failed to create the device\n");
        ret = PTR_ERR(dev_device);
        goto device_error;
    }

    printk(KERN_INFO "CDEV: Driver added and device node /dev/%s created automatically!\n", DEVICE_NAME);
    return 0;

    // 錯誤處理 (goto rollback機制，避免資源洩漏)
device_error:
    class_destroy(dev_class);
class_error:
    cdev_del(&et_cdev);
    unregister_chrdev_region(dev_num, 1);
    return ret;
}

// --- 卸載函數 ---
static void __exit cdev_driver_exit(void) {
    // 1. 移除 Device Node (消滅實體)
    device_destroy(dev_class, dev_num);
    
    // 2. 移除 Class (消滅類別)
    class_destroy(dev_class);
    
    // 3. 從系統中移除 cdev
    cdev_del(&et_cdev);
    
    // 4. 註銷設備號 (把號碼牌還給核心)
    unregister_chrdev_region(dev_num, 1);
    
    printk(KERN_INFO "CDEV: Goodbye! Device node removed automatically.\n");
}

// --- 檔案操作實作 ---

static int dev_open(struct inode *inodep, struct file *filep) {
    printk(KERN_INFO "CDEV: Device has been opened\n");
    return 0;
}

static ssize_t dev_read(struct file *filep, char __user *buffer, size_t len, loff_t *offset) {
    int error_count = 0;

    if (*offset >= size_of_message) return 0;
    if (len > size_of_message - *offset) len = size_of_message - *offset;

    error_count = copy_to_user(buffer, kernel_buffer + *offset, len);

    if (error_count == 0) {
        printk(KERN_INFO "CDEV: Sent %zu characters to the user\n", len);
        *offset += len;
        return len;
    } else {
        printk(KERN_INFO "CDEV: Failed to send %d characters to the user\n", error_count);
        return -EFAULT;
    }
}

static ssize_t dev_write(struct file *filep, const char __user *buffer, size_t len, loff_t *offset) {
    if (len > BUFFER_SIZE - 1) len = BUFFER_SIZE - 1;

    if (copy_from_user(kernel_buffer, buffer, len) != 0) return -EFAULT;

    kernel_buffer[len] = '\0';
    size_of_message = len;
    
    printk(KERN_INFO "CDEV: Received %zu characters from the user\n", len);
    return len;
}

static int dev_release(struct inode *inodep, struct file *filep) {
    printk(KERN_INFO "CDEV: Device successfully closed\n");
    return 0;
}

module_init(cdev_driver_init);
module_exit(cdev_driver_exit);