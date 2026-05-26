/**
 * @file   test_et_app.c
 * @author Frank Huang
 * @date   2026-03-25
 * @brief  A Linux user space program that communicates with the et_driver.c LKM
 */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#define BUFFER_LENGTH 1024
static char receive[BUFFER_LENGTH];

int main() {
    int ret, fd;
    char stringToSend[BUFFER_LENGTH];

    printf("Starting device test code example...\n");
    
    // 1. Open the device
    // 確保您已經執行 mknod 建立了 /dev/et_device
    fd = open("/dev/et_device", O_RDWR);
    if (fd < 0) {
        perror("Failed to open the device /dev/et_device (Did you load the module and mknod?)");
        return errno;
    }

    // 2. Input data
    printf("Type in a short string to send to the kernel module:\n");
    // read line including spaces
    if (fgets(stringToSend, BUFFER_LENGTH, stdin) == NULL) {
        perror("Failed to read input");
        close(fd);
        return errno;
    }
    // Remove newline char
    stringToSend[strcspn(stringToSend, "\n")] = 0;

    // 3. Write to device
    printf("Writing message to the device [%s] (%lu bytes).\n", stringToSend, strlen(stringToSend));
    ret = write(fd, stringToSend, strlen(stringToSend));
    if (ret < 0) {
        perror("Failed to write the message to the device.");
        close(fd);
        return errno;
    }

    printf("Press ENTER to read back from the device...\n");
    getchar();

    // 4. Read from device
    // IMPORTANT: 因為剛剛寫入過，檔案指標 (f_pos) 現在位於資料末端。
    // 我們需要用 lseek 把指標移回開頭，才能讀到剛剛寫入的資料。
    // 如果不這樣做，read 會因為 offset >= size 而回傳 0 (EOF)。
    printf("Resetting file position to beginning...\n");
    if (lseek(fd, 0, SEEK_SET) < 0) {
        perror("Failed to lseek");
        // 嘗試方案 B: 關閉重開
        close(fd);
        fd = open("/dev/et_device", O_RDWR);
    }

    printf("Reading from the device...\n");
    // 清空接收 buffer
    memset(receive, 0, BUFFER_LENGTH);
    ret = read(fd, receive, BUFFER_LENGTH);
    if (ret < 0) {
        perror("Failed to read the message from the device.");
        close(fd);
        return errno;
    }

    printf("The received message is: [%s]\n", receive);
    
    // 5. Close
    close(fd);
    printf("End of the program\n");
    
    return 0;
}
