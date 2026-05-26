# Linux Kernel Driver Learning Journey 🐧

> A hands-on series of **Loadable Kernel Modules (LKM)** built from scratch on **Raspberry Pi 5**,
> covering character devices, GPIO hardware control, interrupt handling, and kernel concurrency.

**Author:** Frank Huang &nbsp;|&nbsp; **Platform:** Raspberry Pi 5 (RP1 GPIO controller) &nbsp;|&nbsp; **Kernel:** 6.x

---

## Modules Overview

| # | Module | Key Concepts |
|:--|:-------|:-------------|
| 01 | [hello_world](#01-hello-world) | LKM lifecycle, `printk`, `Makefile` |
| 02 | [char_driver](#02-char-driver-classic) | `register_chrdev`, `file_operations`, `copy_to/from_user` |
| 02.1 | [cdev_driver](#021-cdev-driver-modern) | `alloc_chrdev_region`, `cdev_init`, `class_create`, udev auto `/dev` node |
| 03 | [gpio_driver](#03-gpio-driver-output) | GPIO output, `gpio_request`, `/dev` write-controlled LED |
| 04 | [gpio_input](#04-gpio-input--interrupt) | GPIO input, `request_irq`, ISR, rising-edge trigger |
| 05 | [led_blinking](#05-led-blinking--sysfs) | Kernel timers, sysfs attribute, runtime frequency control |
| 06 | [mutex_locking](#06-mutex-locking) | Race condition demo, `mutex_init/lock/unlock`, concurrent access safety |

---

## Module Details

### 01 Hello World
The entry point for LKM development. Demonstrates the module lifecycle (`__init` / `__exit`),
kernel logging via `printk`, and the cross-compilation `Makefile` workflow.

```bash
make && sudo insmod hello.ko
sudo dmesg | tail -5
sudo rmmod hello
```

---

### 02 Char Driver (Classic)
Implements an **echo device** (`/dev/et_driver`) using the legacy `register_chrdev` API.
A userspace app writes a string and reads it back — teaching `file_operations`, major number
allocation, and safe kernel↔user data transfer via `copy_to_user` / `copy_from_user`.

---

### 02.1 cdev Driver (Modern)
Upgrades to the **modern character device API**:

- `alloc_chrdev_region` — dynamic Major/Minor number allocation
- `cdev_init` + `cdev_add` — proper cdev registration
- `class_create` + `device_create` — udev integration for **automatic `/dev` node creation**

No more manual `mknod`! This is the production-standard approach used in mainline kernel drivers.

---

### 03 GPIO Driver (Output)
Controls a physical **LED via `/dev` write**. Highlights:

- `gpio_request` / `gpio_direction_output` — GPIO resource management
- Raspberry Pi 5 quirk: RP1 chip remaps GPIO numbers (e.g., GPIO 17 → gpio-586); handled correctly
- Writing `'1'` to `/dev/my_led` turns LED on; `'0'` turns it off

```bash
echo "1" | sudo tee /dev/my_led   # LED ON
echo "0" | sudo tee /dev/my_led   # LED OFF
```

---

### 04 GPIO Input + Interrupt
Replaces polling with a proper **hardware interrupt**:

- `gpio_to_irq` — maps GPIO pin to IRQ number
- `request_irq(IRQF_TRIGGER_RISING)` — registers ISR on rising edge
- ISR increments a counter and logs to `dmesg` — executes in interrupt context (no sleep allowed)

```bash
sudo insmod gpio_input.ko
# Press button → watch dmesg
sudo dmesg -w
```

---

### 05 LED Blinking + Sysfs
Combines a **kernel timer** with a **sysfs attribute** for runtime control:

- Auto-blinking LED driven by `mod_timer`
- Blink frequency adjustable at runtime via `/sys/` — no recompile needed

```bash
# Change blink rate on the fly
echo 500 | sudo tee /sys/kernel/my_led/blink_ms
```

---

### 06 Mutex Locking
Demonstrates the **race condition problem** and its solution:

- A shared counter (`race_car` device) is accessed concurrently by multiple processes
- Shows data corruption without locking
- Fixes it with `mutex_init` / `mutex_lock` / `mutex_unlock`
- `mutex_destroy` called on exit — clean resource handling

---

## Environment

| Item | Detail |
|:-----|:-------|
| Board | Raspberry Pi 5 |
| OS | Raspberry Pi OS (Debian Bookworm) |
| Kernel | 6.x |
| GPIO controller | RP1 (dynamic GPIO base offset — see notes in each driver) |
| Toolchain | `gcc`, `make`, `raspberrypi-kernel-headers` |

### Setup

```bash
# Install kernel headers
sudo apt update
sudo apt install raspberrypi-kernel-headers build-essential

# Build any module
cd 01_hello_world
make

# Load / unload
sudo insmod hello.ko
sudo rmmod hello

# Monitor kernel log
sudo dmesg -w
```

---

## Key Kernel APIs Covered

| API | Purpose |
|:----|:--------|
| `printk` | Kernel-space logging |
| `register_chrdev` / `unregister_chrdev` | Legacy char device registration |
| `alloc_chrdev_region` + `cdev_*` | Modern char device registration |
| `class_create` / `device_create` | udev auto `/dev` node generation |
| `copy_to_user` / `copy_from_user` | Safe kernel↔userspace data transfer |
| `gpio_request` / `gpio_direction_*` | GPIO resource management |
| `gpio_to_irq` / `request_irq` | Hardware interrupt registration |
| `mutex_init` / `mutex_lock` / `mutex_unlock` | Kernel mutual exclusion |
| `mod_timer` | Kernel timer for deferred/periodic work |

---

## Roadmap

| # | Module | Status |
|:--|:-------|:-------|
| 07 | [enc28j60_driver](./07_enc28j60_driver) — SPI Ethernet (MAC+PHY), net_device, sk_buff, NAPI | 🔨 In Progress |
| 08 | [esp32_wifi_driver](./08_esp32_wifi_driver) — ESP32 WiFi Coprocessor, mac80211, SPI protocol | 🔨 In Progress |
| 09 | Platform Device & Device Tree Overlay | 📋 Planned |
| 10 | DMA buffer management | 📋 Planned |
| 11 | IOCTL interface design | 📋 Planned |

---

*Built by Frank Huang on Raspberry Pi 5. Learning notes + production-quality code style.*
