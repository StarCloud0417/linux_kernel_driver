# Graph Report - /home/star/GitHub/linux_kernel_driver  (2026-05-08)

## Corpus Check
- Corpus is ~5,864 words - fits in a single context window. You may not need a graph.

## Summary
- 85 nodes · 82 edges · 18 communities
- Extraction: 89% EXTRACTED · 11% INFERRED · 0% AMBIGUOUS · INFERRED: 9 edges (avg confidence: 0.86)
- Token cost: 0 input · 0 output

## Community Hubs (Navigation)
- [[_COMMUNITY_GPIO and IRQ Drivers|GPIO and IRQ Drivers]]
- [[_COMMUNITY_Char Driver File Operations|Char Driver File Operations]]
- [[_COMMUNITY_Cdev Driver Modern API|Cdev Driver Modern API]]
- [[_COMMUNITY_Hello World Semantic|Hello World Semantic]]

## God Nodes (most connected - your core abstractions)
1. `Cdev Driver` - 7 edges
2. `GPIO Input Driver` - 7 edges
3. `Linux Kernel Module Pattern` - 7 edges
4. `GPIO Output Driver` - 6 edges
5. `LED Blinking Driver` - 6 edges
6. `ET Char Driver` - 5 edges
7. `file_operations struct` - 5 edges
8. `Mutex Locking Driver` - 5 edges
9. `copy_to_user / copy_from_user` - 4 edges
10. `gpio_request / gpio_direction_output` - 3 edges

## Surprising Connections (you probably didn't know these)
- `Hello World Module` --implements--> `Linux Kernel Module Pattern`  [EXTRACTED]
  01_hello_world/hello.c → README.md
- `ET Char Driver` --implements--> `Linux Kernel Module Pattern`  [EXTRACTED]
  02_char_driver/et_driver.c → README.md
- `Cdev Driver` --semantically_similar_to--> `ET Char Driver`  [INFERRED] [semantically similar]
  02_01_cdev_driver/cdev_driver.c → 02_char_driver/et_driver.c
- `alloc_chrdev_region` --semantically_similar_to--> `register_chrdev`  [INFERRED] [semantically similar]
  02_01_cdev_driver/cdev_driver.c → 02_char_driver/et_driver.c
- `Cdev Driver` --implements--> `Linux Kernel Module Pattern`  [EXTRACTED]
  02_01_cdev_driver/cdev_driver.c → README.md

## Hyperedges (group relationships)
- **Character Driver Family** — etdrv_et_driver, cdev_cdev_driver, mutex_mutex_driver [INFERRED 0.90]
- **GPIO Platform Driver Family** — gpio_gpio_driver, gpioIn_gpio_input, led_led_blink [INFERRED 0.90]
- **Kernel-Userspace Data Bridge** — shared_copy_to_user, etdrv_et_driver, cdev_cdev_driver, mutex_mutex_driver [INFERRED 0.85]

## Communities (18 total, 0 thin omitted)

### Community 0 - "GPIO and IRQ Drivers"
Cohesion: 0.18
Nodes (16): gpio_direction_input, GPIO Input Driver, IRQ Interrupt Handler, request_irq / gpio_to_irq, GPIO Output Driver, gpio_request / gpio_direction_output, of_get_named_gpio (Device Tree), platform_driver struct (+8 more)

### Community 1 - "Char Driver File Operations"
Cohesion: 0.28
Nodes (9): et_open, et_read, et_write, file_operations struct, Mutex Locking Driver, mutex_lock / mutex_unlock, Race Condition Prevention, Shared Kernel Buffer (+1 more)

### Community 2 - "Cdev Driver Modern API"
Cohesion: 0.29
Nodes (8): alloc_chrdev_region, Cdev Driver, cdev struct (kernel cdev API), device_create / class_create, Cdev Driver Test App, ET Char Driver, register_chrdev, ET Driver Test App

### Community 10 - "Hello World Semantic"
Cohesion: 0.67
Nodes (3): hello_exit, hello_init, printk Kernel Logging

## Knowledge Gaps
- **15 isolated node(s):** `hello_init`, `hello_exit`, `module_init / module_exit Macros`, `et_open`, `ET Driver Test App` (+10 more)
  These have ≤1 connection - possible missing edges or undocumented components.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `Linux Kernel Module Pattern` connect `GPIO and IRQ Drivers` to `Char Driver File Operations`, `Cdev Driver Modern API`?**
  _High betweenness centrality (0.087) - this node is a cross-community bridge._
- **Why does `Cdev Driver` connect `Cdev Driver Modern API` to `GPIO and IRQ Drivers`, `Char Driver File Operations`?**
  _High betweenness centrality (0.038) - this node is a cross-community bridge._
- **Why does `Mutex Locking Driver` connect `Char Driver File Operations` to `GPIO and IRQ Drivers`?**
  _High betweenness centrality (0.037) - this node is a cross-community bridge._
- **Are the 2 inferred relationships involving `GPIO Input Driver` (e.g. with `Device Tree (DTS/DTB)` and `GPIO Output Driver`) actually correct?**
  _`GPIO Input Driver` has 2 INFERRED edges - model-reasoned connections that need verification._
- **Are the 2 inferred relationships involving `LED Blinking Driver` (e.g. with `gpio_request / gpio_direction_output` and `Device Tree (DTS/DTB)`) actually correct?**
  _`LED Blinking Driver` has 2 INFERRED edges - model-reasoned connections that need verification._
- **What connects `hello_init`, `hello_exit`, `module_init / module_exit Macros` to the rest of the system?**
  _15 weakly-connected nodes found - possible documentation gaps or missing edges._