obj-m += et_driver.o

# 使用預設的核心原始碼路徑（適用於大多數 Linux 發行版）
KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all:
	make -C $(KDIR) M=$(PWD) modules
	$(CC) test_et_app.c -o test_et_app

clean:
	make -C $(KDIR) M=$(PWD) clean
	rm test_et_app
