#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/delay.h>

/* SPI opcodes */
#define ENC28J60_READ_CTRL_REG  0x00
#define ENC28J60_WRITE_CTRL_REG 0x40
#define ENC28J60_BIT_FIELD_SET  0x80
#define ENC28J60_BIT_FIELD_CLR  0xA0
#define ENC28J60_SOFT_RESET     0xFF

/*
 * Register address encoding (同官方 enc28j60_hw.h)
 *   bits[4:0] = register address within bank
 *   bits[6:5] = bank number (0-3)
 *   bit[7]    = SPRD_MASK: MAC/MII register, needs dummy read byte
 */
#define ADDR_MASK  0x1F
#define BANK_MASK  0x60
#define SPRD_MASK  0x80

/* All-bank registers (addr 0x1B~0x1F，任何 bank 都能直接存取，不用切 bank) */
#define EIE    0x1B
#define EIR    0x1C
#define ESTAT  0x1D
#define ECON2  0x1E
#define ECON1  0x1F

/* ECON1 bank select bits */
#define ECON1_BSEL0  0x01
#define ECON1_BSEL1  0x02

/* ESTAT bits */
#define ESTAT_CLKRDY  0x01

/* Bank 0 registers：RX/TX buffer 指標 */
#define ERDPTL  0x00   /* Read Pointer Low  */
#define ERDPTH  0x01   /* Read Pointer High */
#define ETXSTL  0x04   /* TX Start Low  */
#define ETXSTH  0x05   /* TX Start High */
#define ETXNDL  0x06   /* TX End Low  */
#define ETXNDH  0x07   /* TX End High */
#define ERXSTL  0x08   /* RX Start Low */
#define ERXSTH  0x09   /* RX Start High */
#define ERXNDL  0x0A   /* RX End Low */
#define ERXNDH  0x0B   /* RX End High */
#define ERXRDPTL 0x0C  /* RX Read Pointer Low */
#define ERXRDPTH 0x0D  /* RX Read Pointer High */

/* Bank 1 registers */
#define ERXFCON (0x18 | 0x20)  /* RX filter control */

/* Bank 2 registers（MAC，需 dummy byte → SPRD_MASK=0x80） */
#define MACON1  (0x00 | 0x40 | 0x80)
#define MACON3  (0x02 | 0x40 | 0x80)
#define MACON4  (0x03 | 0x40 | 0x80)
#define MABBIPG (0x04 | 0x40 | 0x80)  /* back-to-back inter-packet gap */
#define MAIPGL  (0x06 | 0x40 | 0x80)  /* non-back-to-back IPG low */
#define MAIPGH  (0x07 | 0x40 | 0x80)  /* non-back-to-back IPG high */
#define MAMXFLL (0x0A | 0x40 | 0x80)  /* max frame length low */
#define MAMXFLH (0x0B | 0x40 | 0x80)  /* max frame length high */

/* Bank 3 registers */
#define MAADR5  (0x00 | 0x60 | 0x80)  /* MAC address byte 5（先傳，對應 OUI byte 0） */
#define MAADR6  (0x01 | 0x60 | 0x80)
#define MAADR3  (0x02 | 0x60 | 0x80)
#define MAADR4  (0x03 | 0x60 | 0x80)
#define MAADR1  (0x04 | 0x60 | 0x80)
#define MAADR2  (0x05 | 0x60 | 0x80)
#define EREVID  (0x12 | 0x60)          /* revision ID */

/* MACON1 bits */
#define MACON1_MARXEN  0x01  /* enable MAC RX */
#define MACON1_TXPAUS  0x08  /* enable pause frame TX */
#define MACON1_RXPAUS  0x04  /* enable pause frame RX */

/* MACON3 bits */
#define MACON3_PADCFG0 0x20  /* auto-pad to 60 bytes + CRC */
#define MACON3_TXCRCEN 0x10  /* auto-append CRC */
#define MACON3_FRMLNEN 0x02  /* frame length error check */
#define MACON3_FULDPX  0x01  /* full duplex */

/* MACON4 bits */
#define MACON4_DEFER   0x40  /* defer TX if medium busy（half duplex 用） */

/* ERXFCON bits */
#define ERXFCON_UCEN   0x80  /* unicast filter */
#define ERXFCON_CRCEN  0x20  /* CRC check */
#define ERXFCON_BCEN   0x01  /* broadcast filter */

/* ECON1 bits */
#define ECON1_RXEN     0x04  /* enable packet reception */
#define ECON1_TXRTS    0x08  /* transmit request to send */

/* RX/TX buffer 分配（共 8KB：0x0000 ~ 0x1FFF）
 * RX: 0x0000 ~ 0x19FF（6656 bytes）
 * TX: 0x1A00 ~ 0x1FFF（1536 bytes，約 1 個最大 frame）
 * 注意：ERXST 必須是偶數（datasheet errata #3）
 */
#define RXSTART  0x0000
#define RXEND    0x19FF
#define TXSTART  0x1A00

struct enc28j60_priv {
	struct spi_device *spi;
	u8 bank;  /* 目前選中的 bank，初始值 0xFF 代表未知 */
	u8 spi_buf[4];  /* 共用 SPI 傳輸 buffer（避免 stack DMA 問題） */
};

/*
 * 基本 SPI 寫操作：送 [op|addr, val] 2 bytes
 */
static int spi_write_op(struct enc28j60_priv *priv, u8 op, u8 addr, u8 val)
{
	priv->spi_buf[0] = op | (addr & ADDR_MASK);
	priv->spi_buf[1] = val;
	return spi_write(priv->spi, priv->spi_buf, 2);
}

/*
 * 基本 SPI 讀操作：
 *   ETH register（SPRD_MASK=0）：送 1 byte opcode，讀 1 byte data
 *   MAC/MII register（SPRD_MASK=1）：送 1 byte opcode，讀 2 bytes（dummy + data）
 *
 * 使用 spi_write_then_read（同官方實作），比 full-duplex 更相容各 SPI controller
 */
static u8 spi_read_op(struct enc28j60_priv *priv, u8 op, u8 addr)
{
	u8 tx = op | (addr & ADDR_MASK);
	u8 rx[2];
	/* MAC/MII register 需要多讀一個 dummy byte */
	int rlen = (addr & SPRD_MASK) ? 2 : 1;

	spi_write_then_read(priv->spi, &tx, 1, rx, rlen);
	return rx[rlen - 1];
}

/*
 * 切換 bank
 * 官方做法：register address 本身就帶 bank 資訊（bits[6:5]）
 * 對 all-bank registers（EIE ~ ECON1），直接跳過不切換
 */
static void enc28j60_set_bank(struct enc28j60_priv *priv, u8 addr)
{
	u8 b = (addr & BANK_MASK) >> 5;

	/* all-bank registers 不需要切換 */
	if (addr >= EIE && addr <= ECON1)
		return;

	/* 只改有差異的 bit，減少 SPI 傳輸次數 */
	if ((b & ECON1_BSEL0) != (priv->bank & ECON1_BSEL0)) {
		if (b & ECON1_BSEL0)
			spi_write_op(priv, ENC28J60_BIT_FIELD_SET, ECON1, ECON1_BSEL0);
		else
			spi_write_op(priv, ENC28J60_BIT_FIELD_CLR, ECON1, ECON1_BSEL0);
	}
	if ((b & ECON1_BSEL1) != (priv->bank & ECON1_BSEL1)) {
		if (b & ECON1_BSEL1)
			spi_write_op(priv, ENC28J60_BIT_FIELD_SET, ECON1, ECON1_BSEL1);
		else
			spi_write_op(priv, ENC28J60_BIT_FIELD_CLR, ECON1, ECON1_BSEL1);
	}
	priv->bank = b;
}

/* 寫 register（自動切 bank） */
static int write_reg(struct enc28j60_priv *priv, u8 addr, u8 val)
{
	enc28j60_set_bank(priv, addr);
	return spi_write_op(priv, ENC28J60_WRITE_CTRL_REG, addr, val);
}

/* 寫 16-bit register（低 byte 先，高 byte 後） */
static int write_reg16(struct enc28j60_priv *priv, u8 addrl, u16 val)
{
	int ret;
	ret = write_reg(priv, addrl, val & 0xFF);
	if (ret)
		return ret;
	return write_reg(priv, addrl + 1, val >> 8);
}

/* 讀 register（自動切 bank） */
static u8 read_reg(struct enc28j60_priv *priv, u8 addr)
{
	enc28j60_set_bank(priv, addr);
	return spi_read_op(priv, ENC28J60_READ_CTRL_REG, addr);
}

/* Soft Reset
 * 官方 errata workaround #1：CLKRDY 不可靠，改等固定 2ms
 */
static void enc28j60_soft_reset(struct enc28j60_priv *priv)
{
	spi_write_op(priv, ENC28J60_SOFT_RESET, 0, ENC28J60_SOFT_RESET);
	udelay(2000);  /* errata workaround：等 2ms */
	priv->bank = 0xFF;  /* reset 後 bank 回 0，標記為未知強制下次重設 */
}

/*
 * hw_init — 完整初始化 ENC28J60（Step 2b）
 * 參考 datasheet Section 6 + 官方 enc28j60.c
 * 參數 mac_addr：6 bytes MAC address
 */
static int enc28j60_hw_init(struct enc28j60_priv *priv, const u8 *mac_addr)
{
	/* 1. RX buffer：0x0000 ~ 0x19FF
	 * 注意：ERXST 必須設偶數（errata #3），0x0000 符合 */
	write_reg16(priv, ERXSTL, RXSTART);
	write_reg16(priv, ERXNDL, RXEND);
	/* ERXRDPT 初始指向 RXEND（odd value errata #14：必須寫奇數） */
	write_reg16(priv, ERXRDPTL, RXEND);

	/* 2. TX buffer：0x1A00 ~ 0x1FFF（probe 時先不設 TXND，TX 時再設） */
	write_reg16(priv, ETXSTL, TXSTART);

	/* 3. RX filter：接受 unicast + broadcast，檢查 CRC */
	write_reg(priv, ERXFCON, ERXFCON_UCEN | ERXFCON_CRCEN | ERXFCON_BCEN);

	/* 4. MAC 初始化 */
	/* MACON1：啟用 MAC RX + pause frame */
	write_reg(priv, MACON1, MACON1_MARXEN | MACON1_TXPAUS | MACON1_RXPAUS);

	/* MACON3：自動 pad 到 60 bytes、自動 CRC、frame 長度檢查
	 * full duplex = 1 */
	write_reg(priv, MACON3,
		MACON3_PADCFG0 | MACON3_TXCRCEN | MACON3_FRMLNEN | MACON3_FULDPX);

	/* MACON4：half duplex 模式需要 DEFER，full duplex 不需要，但官方也設了 */
	write_reg(priv, MACON4, MACON4_DEFER);

	/* MAMXFL：最大 frame 長度 1518（含 4-byte CRC） */
	write_reg16(priv, MAMXFLL, 1518);

	/* MABBIPG：full duplex back-to-back IPG = 0x15（datasheet 推薦值） */
	write_reg(priv, MABBIPG, 0x15);

	/* MAIPG：non-back-to-back IPG，datasheet 推薦 low=0x12, high=0x0C */
	write_reg(priv, MAIPGL, 0x12);
	write_reg(priv, MAIPGH, 0x0C);

	/* 5. MAC address（ENC28J60 byte order：MAADR5 先寫，對應 mac[0]）
	 * datasheet 說法：MAADR1 是 OUI 第一個 byte（mac_addr[0]）
	 * 官方 driver 的對應：MAADR5=mac[0], MAADR6=mac[1], ..., MAADR1=mac[4], MAADR2=mac[5]
	 * 等等... 實際上官方 driver 是 MAADR1=byte5, MAADR2=byte6（就是倒著）
	 * 我們照官方：MAADR5 → mac[0], MAADR3 → mac[2], MAADR1 → mac[4] */
	write_reg(priv, MAADR5, mac_addr[0]);
	write_reg(priv, MAADR6, mac_addr[1]);
	write_reg(priv, MAADR3, mac_addr[2]);
	write_reg(priv, MAADR4, mac_addr[3]);
	write_reg(priv, MAADR1, mac_addr[4]);
	write_reg(priv, MAADR2, mac_addr[5]);

	/* 6. 開啟 RX（設 ECON1.RXEN） */
	spi_write_op(priv, ENC28J60_BIT_FIELD_SET, ECON1, ECON1_RXEN);

	pr_info("enc28j60: hw_init 完成，MAC=%pM，RXEN 已開啟\n", mac_addr);
	return 0;
}

static int enc28j60_probe(struct spi_device *spi)
{
	struct enc28j60_priv *priv;
	u8 erevid;
	int ret;

	pr_info("enc28j60: probe — SPI bus=%u CS=%u\n",
		spi->controller->bus_num, spi_get_chipselect(spi, 0));

	priv = devm_kzalloc(&spi->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;
	priv->spi  = spi;
	priv->bank = 0xFF;
	spi_set_drvdata(spi, priv);

	/* SPI 參數設定（DTS 會設 max_speed_hz，這裡保守用 4MHz 避免 race） */
	spi->mode          = SPI_MODE_0;
	spi->bits_per_word = 8;
	ret = spi_setup(spi);
	if (ret) {
		pr_err("enc28j60: spi_setup 失敗: %d\n", ret);
		return ret;
	}

	enc28j60_soft_reset(priv);
	/* reset 後 chip 是 bank 0，明確設定避免 set_bank 誤判 */
	priv->bank = 0;

	/* 讀 EREVID 驗證 SPI 通訊（官方 driver 同樣做法）
	 * EREVID = Bank3 reg 0x12，正常晶片回傳 0x05 或 0x06 */
	erevid = read_reg(priv, EREVID);
	pr_info("enc28j60: EREVID = 0x%02X\n", erevid);

	if (erevid == 0x05 || erevid == 0x06) {
		pr_info("enc28j60: SPI 通訊正常，晶片版本 B%d ✓\n", erevid);
	} else {
		pr_warn("enc28j60: EREVID 異常 (0x%02X)，SPI 通訊可能有問題\n", erevid);
		pr_warn("enc28j60: 0xFF=MISO 恆高(未接)；0x00=MISO 恆低；0x40=SPI 問題\n");
		return -EIO;
	}

	/* Step 2b：hw_init（使用固定測試 MAC，Step 3 改從 DTS/random 取） */
	{
		static const u8 test_mac[6] = { 0x02, 0x42, 0xAC, 0x11, 0x00, 0x01 };
		ret = enc28j60_hw_init(priv, test_mac);
		if (ret)
			return ret;
	}

	return 0;
}

static void enc28j60_remove(struct spi_device *spi)
{
	pr_info("enc28j60: remove\n");
}

static const struct of_device_id enc28j60_of_match[] = {
	{ .compatible = "microchip,enc28j60" },
	{ }
};
MODULE_DEVICE_TABLE(of, enc28j60_of_match);

static struct spi_driver enc28j60_driver = {
	.driver = {
		.name           = "enc28j60",
		.of_match_table = enc28j60_of_match,
	},
	.probe  = enc28j60_probe,
	.remove = enc28j60_remove,
};
module_spi_driver(enc28j60_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Frank Huang");
MODULE_DESCRIPTION("ENC28J60 SPI Ethernet driver - Step 2b: hw_init RX/TX/MAC/RXEN");
