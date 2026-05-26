// SPDX-License-Identifier: GPL-2.0
/*
 * ENC28J60 SPI Ethernet Driver (Learning Implementation)
 *
 * Author: Frank Huang
 * Platform: Raspberry Pi 5
 *
 * Hardware: ENC28J60 — SPI-interfaced MAC + PHY chip by Microchip
 *
 * Architecture:
 *   RPi GPIO (SPI0) ──SPI──> ENC28J60 ──RJ45──> Network
 *
 * Wiring (SPI0):
 *   ENC28J60 VCC  → Pin 1  (3.3V)
 *   ENC28J60 GND  → Pin 6  (GND)
 *   ENC28J60 SCK  → Pin 23 (GPIO11, SPI0_CLK)
 *   ENC28J60 MOSI → Pin 19 (GPIO10, SPI0_MOSI)
 *   ENC28J60 MISO → Pin 21 (GPIO9,  SPI0_MISO)
 *   ENC28J60 CS   → Pin 24 (GPIO8,  SPI0_CE0)
 *   ENC28J60 INT  → Pin 22 (GPIO25, interrupt)
 *
 * Key Concepts Covered:
 *   - SPI kernel API (spi_write_then_read, spi_sync)
 *   - net_device registration & ndo_* ops
 *   - sk_buff: how the kernel represents packets
 *   - NAPI: modern RX polling mechanism
 *   - ENC28J60 register map & bank switching
 *   - Interrupt-driven RX
 *
 * TODO (implement step by step):
 *   [x] Module skeleton & SPI probe
 *   [ ] Register read/write helpers
 *   [ ] Chip init (MAC address, RX filter, buffer config)
 *   [ ] net_device ops (open, stop, xmit)
 *   [ ] TX path: copy sk_buff to chip SRAM, trigger send
 *   [ ] RX path: interrupt → read packet → netif_rx
 *   [ ] NAPI integration
 *   [ ] PHY link status via MISTAT register
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/spi/spi.h>          /* SPI subsystem */
#include <linux/netdevice.h>        /* net_device, netif_* */
#include <linux/etherdevice.h>      /* eth_type_trans, alloc_etherdev */
#include <linux/skbuff.h>           /* sk_buff — kernel packet buffer */
#include <linux/interrupt.h>        /* request_irq, IRQ_HANDLED */
#include <linux/gpio/consumer.h>    /* gpiod_get, gpiod_to_irq */
#include <linux/delay.h>
#include <linux/of.h>               /* Device Tree */

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Frank Huang");
MODULE_DESCRIPTION("ENC28J60 SPI Ethernet Driver - Learning Implementation");
MODULE_VERSION("0.1");

/* ============================================================
 * ENC28J60 SPI Opcodes
 * ============================================================ */
#define ENC28J60_RCR    0x00    /* Read Control Register */
#define ENC28J60_RBM    0x3A    /* Read Buffer Memory */
#define ENC28J60_WCR    0x40    /* Write Control Register */
#define ENC28J60_WBM    0x7A    /* Write Buffer Memory */
#define ENC28J60_BFS    0x80    /* Bit Field Set */
#define ENC28J60_BFC    0xA0    /* Bit Field Clear */
#define ENC28J60_SRC    0xFF    /* System Reset Command (soft reset) */

/* ============================================================
 * Register Addresses (Bank 0~3, selected via ECON1)
 * ============================================================ */
/* Ethernet Control Registers (common across banks) */
#define EIE     0x1B    /* Interrupt Enable */
#define EIR     0x1C    /* Interrupt Flag */
#define ESTAT   0x1D    /* Status */
#define ECON2   0x1E
#define ECON1   0x1F    /* Control: bank select, TX/RX enable */

/* Bank 0 */
#define ERDPTL  0x00    /* Read Pointer Low */
#define ERDPTH  0x01
#define EWRPTL  0x02    /* Write Pointer Low */
#define EWRPTH  0x03
#define ETXSTL  0x04    /* TX Start Low */
#define ETXSTH  0x05
#define ETXNDL  0x06    /* TX End Low */
#define ETXNDH  0x07
#define ERXSTL  0x08    /* RX Start Low */
#define ERXSTH  0x09
#define ERXNDL  0x0A    /* RX End Low */
#define ERXNDH  0x0B
#define ERXRDPTL 0x0C   /* RX Read Pointer */
#define ERXRDPTH 0x0D

/* Bank 2 — MAC registers (prefix M = MII/MAC) */
#define MACON1  0x00
#define MACON3  0x02
#define MACON4  0x03
#define MABBIPG 0x04    /* Back-to-back inter-packet gap */
#define MAIPGL  0x06
#define MAIPGH  0x07
#define MAMXFLL 0x0A    /* Max frame length low */
#define MAMXFLH 0x0B

/* Bank 3 — MAC address registers */
#define MAADR1  0x04
#define MAADR2  0x05
#define MAADR3  0x02
#define MAADR4  0x03
#define MAADR5  0x00
#define MAADR6  0x01

/* EIE bits */
#define EIE_INTIE   BIT(7)
#define EIE_PKTIE   BIT(6)   /* Packet received interrupt enable */
#define EIE_TXIE    BIT(3)   /* TX complete interrupt enable */
#define EIE_TXERIE  BIT(1)
#define EIE_RXERIE  BIT(0)

/* ECON1 bits */
#define ECON1_TXRTS BIT(3)   /* TX request to send */
#define ECON1_RXEN  BIT(2)   /* RX enable */

/* ============================================================
 * Buffer layout in ENC28J60's 8KB SRAM
 *
 *  0x0000 ┌──────────────┐
 *         │   RX Buffer  │  (6KB)
 *  0x17FF └──────────────┘
 *  0x1800 ┌──────────────┐
 *         │   TX Buffer  │  (2KB)
 *  0x1FFF └──────────────┘
 * ============================================================ */
#define ENC28J60_RXBUF_START    0x0000
#define ENC28J60_RXBUF_END      0x17FF
#define ENC28J60_TXBUF_START    0x1800
#define ENC28J60_TXBUF_END      0x1FFF
#define ENC28J60_MAX_FRAMELEN   1518

/* ============================================================
 * Driver private data
 * ============================================================ */
struct enc28j60_priv {
    struct spi_device   *spi;
    struct net_device   *netdev;
    struct napi_struct   napi;
    struct mutex         lock;      /* protects SPI access */

    struct gpio_desc    *gpio_int;  /* INT pin */
    int                  irq;

    u8                   bank;      /* currently selected register bank */
    u16                  next_pkt;  /* next packet pointer in chip SRAM */
};

/* ============================================================
 * SPI Low-level Helpers (TODO: implement these)
 * ============================================================ */

/**
 * enc28j60_read_reg - Read one byte from a control register
 * @priv: driver private data
 * @reg:  register address (within current bank)
 *
 * SPI transaction: send [opcode | reg], receive 1 byte
 * For MAC/MII registers, there's a dummy byte before real data.
 */
static u8 enc28j60_read_reg(struct enc28j60_priv *priv, u8 reg)
{
    /* TODO */
    return 0;
}

/**
 * enc28j60_write_reg - Write one byte to a control register
 */
static void enc28j60_write_reg(struct enc28j60_priv *priv, u8 reg, u8 val)
{
    /* TODO */
}

/**
 * enc28j60_set_bank - Switch register bank (0~3) via ECON1[1:0]
 */
static void enc28j60_set_bank(struct enc28j60_priv *priv, u8 bank)
{
    /* TODO: BFC to clear, BFS to set */
    priv->bank = bank;
}

/* ============================================================
 * net_device operations
 * ============================================================ */

/**
 * enc28j60_open - Called when interface brought up (ip link set eth0 up)
 */
static int enc28j60_open(struct net_device *dev)
{
    struct enc28j60_priv *priv = netdev_priv(dev);

    pr_info("enc28j60: open\n");

    /* TODO:
     * 1. enc28j60_hw_init() — configure MAC, RX filter, buffer ptrs
     * 2. request_irq(priv->irq, enc28j60_irq_handler, ...)
     * 3. enable RX: write ECON1_RXEN
     * 4. netif_start_queue(dev)
     */

    netif_start_queue(dev);
    return 0;
}

/**
 * enc28j60_stop - Called when interface brought down
 */
static int enc28j60_stop(struct net_device *dev)
{
    struct enc28j60_priv *priv = netdev_priv(dev);

    pr_info("enc28j60: stop\n");
    netif_stop_queue(dev);

    /* TODO: disable RX, free IRQ */

    return 0;
}

/**
 * enc28j60_xmit - Send a packet (called by kernel with sk_buff)
 *
 * sk_buff is the kernel's packet representation.
 * skb->data points to the raw ethernet frame.
 * skb->len is the frame length.
 *
 * Flow:
 *   1. Stop TX queue (one packet at a time for ENC28J60)
 *   2. Write per-packet control byte to TXBUF_START
 *   3. Copy skb->data to chip SRAM via WBM opcode
 *   4. Set ETXND to TXBUF_START + skb->len
 *   5. Set ECON1_TXRTS to trigger hardware send
 *   6. Free sk_buff (dev_kfree_skb)
 */
static netdev_tx_t enc28j60_xmit(struct sk_buff *skb, struct net_device *dev)
{
    /* TODO */
    pr_info("enc28j60: xmit %d bytes\n", skb->len);
    dev_kfree_skb(skb);
    return NETDEV_TX_OK;
}

static const struct net_device_ops enc28j60_netdev_ops = {
    .ndo_open       = enc28j60_open,
    .ndo_stop       = enc28j60_stop,
    .ndo_start_xmit = enc28j60_xmit,
};

/* ============================================================
 * Interrupt Handler (RX notification from chip)
 * ============================================================ */
static irqreturn_t enc28j60_irq_handler(int irq, void *dev_id)
{
    struct enc28j60_priv *priv = dev_id;

    /*
     * TODO:
     * - Read EIR to find out what triggered
     * - If PKTIF set → schedule NAPI poll
     * - If TXIF set → wake TX queue
     * - Clear interrupt flags
     */

    pr_info("enc28j60: interrupt!\n");
    return IRQ_HANDLED;
}

/* ============================================================
 * SPI probe — called when kernel matches this driver to device
 * ============================================================ */
static int enc28j60_probe(struct spi_device *spi)
{
    struct net_device   *netdev;
    struct enc28j60_priv *priv;
    int ret;

    pr_info("enc28j60: probe — SPI bus %d, CS %d\n",
            spi->master->bus_num, spi->chip_select);

    /* 1. Allocate net_device with private data appended */
    netdev = alloc_etherdev(sizeof(struct enc28j60_priv));
    if (!netdev)
        return -ENOMEM;

    priv = netdev_priv(netdev);
    priv->spi    = spi;
    priv->netdev = netdev;
    mutex_init(&priv->lock);
    spi_set_drvdata(spi, priv);

    /* 2. SPI setup — ENC28J60 supports mode 0,0 up to 20 MHz */
    spi->max_speed_hz = 8000000;    /* 8 MHz, safe starting point */
    spi->mode         = SPI_MODE_0;
    spi->bits_per_word = 8;
    ret = spi_setup(spi);
    if (ret) {
        pr_err("enc28j60: spi_setup failed: %d\n", ret);
        goto err_free_netdev;
    }

    /* 3. Set net_device ops and name */
    netdev->netdev_ops = &enc28j60_netdev_ops;
    SET_NETDEV_DEV(netdev, &spi->dev);

    /*
     * 4. TODO: Chip init
     *    - Soft reset (SRC opcode)
     *    - Wait ESTAT.CLKRDY
     *    - Configure RX/TX buffer boundaries
     *    - Set MAC address (read from DT or random)
     *    - Configure MACON registers
     *    - Set up ERXFCON (RX filter)
     */

    /* 5. Register net_device */
    ret = register_netdev(netdev);
    if (ret) {
        pr_err("enc28j60: register_netdev failed: %d\n", ret);
        goto err_free_netdev;
    }

    pr_info("enc28j60: registered as %s\n", netdev->name);
    return 0;

err_free_netdev:
    free_netdev(netdev);
    return ret;
}

static void enc28j60_remove(struct spi_device *spi)
{
    struct enc28j60_priv *priv = spi_get_drvdata(spi);

    unregister_netdev(priv->netdev);
    free_netdev(priv->netdev);
    pr_info("enc28j60: removed\n");
}

/* ============================================================
 * Device Tree matching table
 * Add to your RPi DT overlay:
 *   &spi0 {
 *       enc28j60: enc28j60@0 {
 *           compatible = "microchip,enc28j60";
 *           reg = <0>;
 *           spi-max-frequency = <8000000>;
 *           interrupt-parent = <&gpio>;
 *           interrupts = <25 IRQ_TYPE_EDGE_FALLING>;
 *       };
 *   };
 * ============================================================ */
static const struct of_device_id enc28j60_of_match[] = {
    { .compatible = "microchip,enc28j60" },
    { }
};
MODULE_DEVICE_TABLE(of, enc28j60_of_match);

static const struct spi_device_id enc28j60_spi_ids[] = {
    { "enc28j60", 0 },
    { }
};
MODULE_DEVICE_TABLE(spi, enc28j60_spi_ids);

static struct spi_driver enc28j60_driver = {
    .driver = {
        .name           = "enc28j60",
        .of_match_table = enc28j60_of_match,
    },
    .id_table = enc28j60_spi_ids,
    .probe    = enc28j60_probe,
    .remove   = enc28j60_remove,
};

module_spi_driver(enc28j60_driver);
