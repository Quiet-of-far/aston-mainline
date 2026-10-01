// SPDX-License-Identifier: GPL-2.0-only
/*
 * Driver for the TI TMAG5273 Low-Power Linear 3D Hall-Effect Sensor
 *
 * Copyright (C) 2022 WolfVision GmbH
 *
 * Author: Gerald Loacker <gerald.loacker@wolfvision.net>
 */

#include <linux/bitfield.h>
#include <linux/bits.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>

#define MMC56X3_PRODUCT_ID 0x39
#define MMC56X3_CTRL0_REG 0x1B
#define MMC56X3_CTRL1_REG 0x1C
#define MMC56X3_CTRL2_REG 0x1D
#define MMC56X3_STATUS_REG 0x18
#define MMC56X3_OUT_TEMP 0x09
#define MMC56X3_OUT_X_L 0x00
#define MMC5603_ODR_REG 0x1A
#define MMC56X3_CHIP_ID 0x10
#define MMC56X3_STATUS_MEAS_M_DONE 0x80
#define MMC56X3_STATUS_MEAS_T_DONE 0x40

#define MMC56X3_CMD_RESET              0x10
#define MMC56X3_CMD_SET                0x08
#define MMC56X3_CMD_SW_RESET           0x80
#define MMC56X3_CMD_TAKE_MEAS_M        0x01
#define MMC56X3_CMD_TAKE_MEAS_T        0x02
#define MMC56X3_CMD_AUTO_SELF_RESET_EN 0x20
#define MMC56X3_CMD_CMM_FREQ_EN        0x80
#define MMC56X3_CMD_CMM_EN             0x10
#define MMC56X3_CMD_HPOWER             0x80

enum mmc56x3_axis {
	AXIS_X = 0,
	AXIS_Y,
	AXIS_Z,
};

#define MMC56X3_CHANNEL(_axis) { \
	.type = IIO_MAGN, \
	.modified = 1, \
	.channel2 = IIO_MOD_ ## _axis, \
	.address = AXIS_ ## _axis, \
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW), \
	.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SAMP_FREQ) | \
			BIT(IIO_CHAN_INFO_SCALE), \
}

static const struct iio_chan_spec mmc56x3_channels[] = {
	MMC56X3_CHANNEL(X),
	MMC56X3_CHANNEL(Y),
	MMC56X3_CHANNEL(Z),
};

struct mmc56x3 {
	struct i2c_client *i2c;
	struct regmap *regmap;
	int chip_id;
	int temp;
	int x;
	int y;
	int z;
};

static const struct regmap_config mmc56x3_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0xff,
};

static int mmc56x3_wait_until_ready(struct mmc56x3 *mmc56x3)
{
	int status = 0;
	int ret;

	/* Wait for measurement to be completed */
	do {
		msleep(3);
		ret = regmap_read(mmc56x3->regmap, MMC56X3_STATUS_REG, &status);
		if (ret < 0)
			return ret;
	} while ((status & 0xC0) != (MMC56X3_STATUS_MEAS_M_DONE | MMC56X3_STATUS_MEAS_T_DONE));

	return 0;
}

static int mmc56x3_take_measures(struct mmc56x3 *mmc56x3)
{
	int ret = 0;
	int read;
	u64 temp;
	ret = regmap_write(mmc56x3->regmap, MMC56X3_CTRL0_REG, MMC56X3_CMD_TAKE_MEAS_T);
	if (ret < 0)
		goto error;

	msleep(10);

	ret = regmap_write(mmc56x3->regmap, MMC56X3_CTRL0_REG, MMC56X3_CMD_TAKE_MEAS_M);
	if (ret < 0)
		goto error;

	mmc56x3_wait_until_ready(mmc56x3);

	ret = regmap_read(mmc56x3->regmap, MMC56X3_OUT_TEMP, &read);
	if (ret < 0)
		goto error;

	temp = read;


	temp = temp << 8;
	temp *= 8;
	temp /= 10;
	temp -= 75 << 8;
	temp = (temp + 0x7f) >> 8;

	uint8_t buffer[9];
	uint32_t x;
	uint32_t y;
	uint32_t z;


	regmap_raw_read(mmc56x3->regmap, MMC56X3_OUT_X_L, &buffer, 9);

	x = buffer[0] << 12 | buffer[1] << 4 | buffer[6] >> 4;
	y = buffer[2] << 12 | buffer[3] << 4 | buffer[7] >> 4;
	z = buffer[4] << 12 | buffer[5] << 4 | buffer[8] >> 4;
	// fix center offsets
	x -= 1 << 19;
	y -= 1 << 19;
	z -= 1 << 19;

	mmc56x3->x = x;
	mmc56x3->y = y;
	mmc56x3->z = z;
	mmc56x3->temp = temp;

error:
	return ret;
}

static int mmc56x3_raw_to_mgauss(struct mmc56x3 *mmc56x3, int index, int *val)
{
	switch (index) {
		case AXIS_X:
			*val = (mmc56x3->x) * 1000 / 1024;
			break;
		case AXIS_Y:
			*val = (mmc56x3->y) * 1000 / 1024;
			break;
		case AXIS_Z:
			*val = (mmc56x3->z) * 1000 / 1024;
			break;
		default:
			dev_err(&mmc56x3->i2c->dev, "raw_to_mgaus index was invalid, %d:", index);
			return -EINVAL;
	}

	return 0;
}

static int mmc56x3_read_raw(struct iio_dev *indio_dev,
			     struct iio_chan_spec const *chan, int *val,
			     int *val2, long mask)
{
	struct mmc56x3 *data = iio_priv(indio_dev);
	int ret;

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		mmc56x3_take_measures(data);
		ret = mmc56x3_raw_to_mgauss(data, chan->address, val);
		if (ret < 0)
			return ret;
		return IIO_VAL_INT;
	case IIO_CHAN_INFO_SCALE:
		*val = 1;
		return IIO_VAL_INT;
	case IIO_CHAN_INFO_SAMP_FREQ:
		*val = 524288;
		return IIO_VAL_INT;
	default:
		return -EINVAL;
	}
}

static const struct iio_info mmc56x3_info = {
	.read_raw	= mmc56x3_read_raw,
};

static int mmc56x3_probe(struct i2c_client *i2c)
{
	struct iio_dev *indio_dev;
	struct mmc56x3 *mmc56x3;
	int ret = 0;

	indio_dev = devm_iio_device_alloc(&i2c->dev, sizeof(*mmc56x3));
	if (indio_dev == NULL)
		return -ENOMEM;

	mmc56x3 = iio_priv(indio_dev);
	i2c_set_clientdata(i2c, indio_dev);
	mmc56x3->i2c = i2c;

	indio_dev->info = &mmc56x3_info;
	indio_dev->name = "mmc56x3";
	indio_dev->channels = mmc56x3_channels;
	indio_dev->num_channels = ARRAY_SIZE(mmc56x3_channels);
	indio_dev->modes = INDIO_DIRECT_MODE;

	mmc56x3->regmap = devm_regmap_init_i2c(i2c, &mmc56x3_regmap_config);
	if (IS_ERR(mmc56x3->regmap)) {
		ret = dev_err_probe(&i2c->dev, PTR_ERR(mmc56x3->regmap), "failed to allocate register map\n");
		goto error;
	}

	ret = regmap_read(mmc56x3->regmap, MMC56X3_PRODUCT_ID, &mmc56x3->chip_id);
	if (ret)
		goto error;

	if (mmc56x3->chip_id != 16) {
		ret = -EINVAL;
		goto error;
	}

	ret = regmap_write(mmc56x3->regmap, MMC56X3_CTRL1_REG, MMC56X3_CMD_SW_RESET);
	if (ret < 0)
		goto error;
	msleep(20);

	ret = regmap_write(mmc56x3->regmap, MMC56X3_CTRL0_REG, MMC56X3_CMD_SET);
	if (ret < 0)
		goto error;
	msleep(1);

	ret = regmap_write(mmc56x3->regmap, MMC56X3_CTRL0_REG, MMC56X3_CMD_RESET);
	if (ret < 0)
		goto error;
	msleep(1);


	return devm_iio_device_register(&i2c->dev, indio_dev);
error:
	return ret;
}

static const struct i2c_device_id mmc56x3_id[] = {
	{ "mmc56x3" },
	{}
};
MODULE_DEVICE_TABLE(i2c, mmc56x3_id);

static const struct of_device_id mmc56x3_of_match[] = {
	{ .compatible = "memsic,mmc56x3" },
	{}
};
MODULE_DEVICE_TABLE(of, mmc56x3_of_match);

static struct i2c_driver mmc56x3_driver = {
	.driver	 = {
		.name = "mmc56x3",
		.of_match_table = mmc56x3_of_match,
	},
	.probe = mmc56x3_probe,
	.id_table = mmc56x3_id,
};
module_i2c_driver(mmc56x3_driver);

MODULE_DESCRIPTION("MEMSIC MMC56X3 driver");
MODULE_AUTHOR("Gerald Loacker <gerald.loacker@wolfvision.net>");
MODULE_LICENSE("GPL");
