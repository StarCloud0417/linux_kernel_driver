# Makefile 用於自動化編譯流程

# obj-m 表示我們要建立一個模組 (module)。
# hello.o 對應到原始碼 hello.c
obj-m += hello.o

# KDIR 指向核心原始碼或標頭檔的位置
# 我們使用 `uname -r` 自動抓取當前執行的核心版本
KDIR := /lib/modules/$(shell uname -r)/build

# PWD 是當前目錄
PWD := $(shell pwd)

# default 目標：執行編譯
# -C $(KDIR): 切換到核心原始碼目錄，借用核心的 Makefile 系統
# M=$(PWD): 告訴核心，要編譯的模組原始碼在我們目前的目錄
# modules: 這是核心 Makefile 定義的目標，專門用來編譯模組
all:
	make -C $(KDIR) M=$(PWD) modules

# clean 目標：清除編譯產生的暫存檔
clean:
	make -C $(KDIR) M=$(PWD) clean
