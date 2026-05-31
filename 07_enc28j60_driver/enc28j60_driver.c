#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/delay.h>

/* SPI opcodes */
#define ENC28J60_READ_CTRL_REG  0x00
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

/* Bank 3 registers */
#define EREVID  (0x12 | 0x60)  /* revision ID；bank = (0x60>>5) = 3 */

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
MODULE_AUTHOR("Star");
MODULE_DESCRIPTION("ENC28J60 SPI Ethernet driver - Step 2a: SPI helpers + EREVID verify");
