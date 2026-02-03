#include <linux/init.h>   // 包含模組初始化的巨集 (module_init, module_exit)
#include <linux/module.h> // 包含核心模組的必要機制 (MODULE_LICENSE 等)
#include <linux/kernel.h> // 包含核心常用函數 (如 printk)

// 1. 模組資訊 (Metadata)
// 這對核心來說很重要，標示了這個模組的許可證、作者等資訊。
// 如果沒有 GPL 宣告，某些核心功能可能會拒絕被此模組調用 (Tainted Kernel)。
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Frank Huang");
MODULE_DESCRIPTION("A simple Hello World Kernel Module");
MODULE_VERSION("0.1");

// 2. 初始化函數 (Entrance)
// 當我們執行 `insmod` 時，核心會呼叫這個函數。
// __init 是一個修飾詞，告訴核心這個函數只在初始化時用一次，用完就可以釋放記憶體。
static int __init hello_init(void) {
    // printk 是核心層級的 printf。
    // KERN_INFO 是訊息等級 (Level)，表示這是一般資訊。
    printk(KERN_INFO "Hello, Kernel! I am Frank's driver.\n");
    return 0; // 回傳 0 表示載入成功。非 0 表示失敗。
}

// 3. 卸載函數 (Exit)
// 當我們執行 `rmmod` 時，核心會呼叫這個函數。
// __exit 修飾詞表示這個函數只在卸載時使用。
static void __exit hello_exit(void) {
    printk(KERN_INFO "Goodbye, Kernel! Logging out.\n");
}

// 4. 註冊函數
// 告訴核心，哪一個函數是入口 (Init)，哪一個是出口 (Exit)。
module_init(hello_init);
module_exit(hello_exit);
