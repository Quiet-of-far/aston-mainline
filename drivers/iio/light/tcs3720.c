// SPDX-License-Identifier: GPL-2.0-only
/*
 * TCS3720 Light Sensor
 *
 * Data sheet: https://look.ams-osram.com/m/3a8b2e5489b51053/original/TCS3720-ALS-Color-and-Proximity-Sensor-for-Behind-OLED-Applications.pdf
 *
 */

#include <linux/module.h>
#include <linux/mod_devicetable.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/util_macros.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/buffer.h>
#include <linux/delay.h>

#define TCS3720_DRV_NAME 	"tcs3720"

#define TCS3720_INTENAB 	0xDD
#define TCS3720_RESET 		0xA8
#define TCS3720_CONFIG 		0x94
#define TCS3720_CHIP_ID 	0x92
#define TCS3720_ENABLE		0x80


struct tcs3720_data {
	struct mutex lock;
	struct regmap *regmap;
	bool interrupt_called;
	u16 green;
	u16 red;
	u16 blue;
	u16 clear;
};

#define TCS3720_CHANNEL(_color, _si) { \
	.type = IIO_INTENSITY, \
	.modified = 1, \
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW), \
	.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE) | \
		BIT(IIO_CHAN_INFO_INT_TIME), \
	.channel2 = IIO_MOD_LIGHT_##_color, \
	.address = _si, \
	.scan_index = _si, \
	.scan_type = { \
		.sign = 'u', \
		.realbits = 16, \
		.storagebits = 16, \
		.endianness = IIO_CPU, \
	}, \
}

static const struct iio_chan_spec tcs3720_channels[] = {
	TCS3720_CHANNEL(BLUE, 0),
	TCS3720_CHANNEL(GREEN, 1),
	TCS3720_CHANNEL(RED, 2),
	TCS3720_CHANNEL(CLEAR, 3),
	IIO_CHAN_SOFT_TIMESTAMP(4),
};

static int tcs3720_read_als_value(struct tcs3720_data *data)
{
	int ret;
	u16 var;

	// Set PON to 1 (Exit Sleep Mode)
	ret = regmap_write(data->regmap, TCS3720_ENABLE, BIT(0));
	if(ret)
		return ret;

	// Wait
	msleep(100);

	// Set COLOR_MODE
	regmap_write(data->regmap, TCS3720_CONFIG, BIT(1) | BIT(2));

	// ALS/Color Interrupt Enable
	regmap_write(data->regmap, TCS3720_INTENAB, BIT(4));

	// Keep PON at 1, Set AWEN to 1, Set AEN to 1
	regmap_write(data->regmap, TCS3720_ENABLE, BIT(0) | BIT(1) | BIT(3));

	while(!data->interrupt_called){
		msleep(10);
	}
	data->interrupt_called = 0;

	// Read Blue channel
	regmap_raw_read(data->regmap, 0xF8, &var, 2);
	data->blue = var;

	// Read Green channel
	regmap_raw_read(data->regmap, 0xFA, &var, 2);
	data->green = var;

	// Read Red channel
	regmap_raw_read(data->regmap, 0xFC, &var, 2);
	data->red = var;

	// Read Clear channel
	regmap_raw_read(data->regmap, 0xFE, &var, 2);
	data->clear = var;

	// SOFT_RESET
	ret = regmap_write(data->regmap, TCS3720_RESET, BIT(0));

	return ret;
}

static int tcs3720_read_raw(struct iio_dev *indio_dev,
			     struct iio_chan_spec const *chan,
			     int *val, int *val2, long mask)
{
	struct tcs3720_data *data = iio_priv(indio_dev);
	int ret;

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		switch (chan->type) {
			case IIO_INTENSITY:
				mutex_lock(&data->lock);
				ret = tcs3720_read_als_value(data);
				mutex_unlock(&data->lock);
				if (ret < 0)
					return ret;
				switch (chan->address) {
					case 0:
						*val = data->blue;
						break;
					case 1:
						*val = data->green;
						break;
					case 2:
						*val = data->red;
						break;
					case 3:
						*val = data->clear;
						break;
					default:
						return -EINVAL;
				}
				return IIO_VAL_INT;
			default:
				return -EINVAL;
		}

	default:
		return -EINVAL;
	}
}

static irqreturn_t tcs3720_interrupt_handler(int irq, void *private)
{
	struct iio_dev *indio_dev = private;
	struct tcs3720_data *data = iio_priv(indio_dev);
	data->interrupt_called = 1;

	return IRQ_HANDLED;
}

static const struct iio_info tcs3720_info = {
	.read_raw		= tcs3720_read_raw,
};

static const struct regmap_config tcs3720_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0xff,
};

static int tcs3720_probe(struct i2c_client *client)
{
	struct tcs3720_data *data;
	struct iio_dev *indio_dev;
	int ret;
	int chip_id = 0;

	indio_dev = devm_iio_device_alloc(&client->dev, sizeof(*data));
	if (!indio_dev)
		return -ENOMEM;
	data = iio_priv(indio_dev);
	data->regmap = devm_regmap_init_i2c(client, &tcs3720_regmap_config);
	if (IS_ERR(data->regmap)) {
		dev_err(&client->dev, "regmap_init failed!\n");
		return PTR_ERR(data->regmap);
	}

	mutex_init(&data->lock);
	indio_dev->info = &tcs3720_info;
	indio_dev->name = TCS3720_DRV_NAME;
	indio_dev->channels = tcs3720_channels;
	indio_dev->num_channels = ARRAY_SIZE(tcs3720_channels);

	// Check revision
	ret = regmap_read(data->regmap, TCS3720_CHIP_ID, &chip_id);
	if(ret)
		return ret;

	if(chip_id != 130){
		return -ENODEV;
	}

	ret = devm_request_threaded_irq(&client->dev, client->irq,
						NULL, tcs3720_interrupt_handler,
						IRQF_TRIGGER_FALLING |
						IRQF_ONESHOT,
						"tcs3720_thresh_event",
						indio_dev);
	if(ret)
		return ret;

	return devm_iio_device_register(&client->dev, indio_dev);
}

static const struct i2c_device_id tcs3720_id[] = {
	{ "tcs3720" },
	{ }
};
MODULE_DEVICE_TABLE(i2c, tcs3720_id);

static struct i2c_driver tcs3720_driver = {
	.driver = {
		.name	= TCS3720_DRV_NAME,
	},
	.probe		= tcs3720_probe,
	.id_table	= tcs3720_id,
};

module_i2c_driver(tcs3720_driver);

MODULE_AUTHOR("Shandorman <98683030+jiganomegsdfdf@users.noreply.github.com>");
MODULE_DESCRIPTION("TCS3720 Light Sensor");
MODULE_LICENSE("GPL v2");
