#include <linux/module.h>
#include <linux/spi/spi.h>

static int enc28j60_probe(struct spi_device *spi)
{
	pr_info("enc28j60: probe 被呼叫，SPI bus=%u CS=%u\n",
		spi->controller->bus_num, spi_get_chipselect(spi, 0));
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
		.name = "enc28j60",
		.of_match_table = enc28j60_of_match,
	},
	.probe = enc28j60_probe,
	.remove = enc28j60_remove,
};
module_spi_driver(enc28j60_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Star");
MODULE_DESCRIPTION("ENC28J60 SPI Ethernet driver - Step 1 skeleton");
