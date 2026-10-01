// SPDX-License-Identifier: GPL-2.0-only
// Copyright (c) 2024 FIXME
// Generated with linux-mdss-dsi-panel-driver-generator from vendor device tree:
//   Copyright (c) 2013, The Linux Foundation. All rights reserved. (FIXME)

#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/regulator/consumer.h>

#include <drm/display/drm_dsc.h>
#include <drm/display/drm_dsc_helper.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_bridge.h>
#include <drm/drm_atomic_state_helper.h>

#include "panel-aa551-mode-commands.h"
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>
#include <drm/drm_probe_helper.h>

#define aa551_dcs_write_seq(dsi, cmd, seq...)                         \
	do {                                                              \
		static const u8 data[] = { cmd, seq };                    \
		int err = mipi_dsi_dcs_write_buffer_chatty(dsi, data,     \
							 ARRAY_SIZE(data));    \
		if (err < 0)                                              \
			return err;                                          \
	} while (0)

struct panel_aa551_p_3_a0004_dsc {
	struct drm_panel panel;
	unsigned int refresh_rate;
	struct mipi_dsi_device *dsi;
	struct drm_dsc_config dsc;
	struct gpio_desc *reset_gpio;

	struct regulator *vddio;
	struct regulator *vci;
};

static inline
struct panel_aa551_p_3_a0004_dsc *to_panel_aa551_p_3_a0004_dsc(struct drm_panel *panel)
{
	return container_of(panel, struct panel_aa551_p_3_a0004_dsc, panel);
}

/* A mode bridge supplies the selected refresh rate to panel preparation. */
struct aa551_mode_bridge {
	struct drm_bridge bridge;
	struct drm_bridge *panel_bridge;
	struct panel_aa551_p_3_a0004_dsc *panel;
};

static int aa551_bridge_attach(struct drm_bridge *bridge,
			      struct drm_encoder *encoder,
			      enum drm_bridge_attach_flags flags)
{
	struct aa551_mode_bridge *ctx = container_of(bridge, struct aa551_mode_bridge, bridge);

	return drm_bridge_attach(encoder, ctx->panel_bridge, bridge, flags);
}

static void aa551_bridge_mode_set(struct drm_bridge *bridge,
				 const struct drm_display_mode *mode,
				 const struct drm_display_mode *adjusted_mode)
{
	struct aa551_mode_bridge *ctx = container_of(bridge, struct aa551_mode_bridge, bridge);

	ctx->panel->refresh_rate = drm_mode_vrefresh(adjusted_mode);
}

static const struct drm_bridge_funcs aa551_mode_bridge_funcs = {
	.attach = aa551_bridge_attach,
	.mode_set = aa551_bridge_mode_set,
	.atomic_reset = drm_atomic_helper_bridge_reset,
	.atomic_duplicate_state = drm_atomic_helper_bridge_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_bridge_destroy_state,
};

static int aa551_set_refresh_rate(struct panel_aa551_p_3_a0004_dsc *ctx)
{
	const struct aa551_mode_command *commands;
	unsigned int count, i;
	int ret;

	switch (ctx->refresh_rate) {
	case 90:
		commands = aa551_mode_90;
		count = ARRAY_SIZE(aa551_mode_90);
		break;
	case 120:
		commands = aa551_mode_120;
		count = ARRAY_SIZE(aa551_mode_120);
		break;
	default:
		/* 30 Hz remains a host pacing mode; vendor DT has no 30 Hz sequence. */
		commands = aa551_mode_60;
		count = ARRAY_SIZE(aa551_mode_60);
		break;
	}
	for (i = 0; i < count; i++) {
		ret = mipi_dsi_dcs_write_buffer_chatty(ctx->dsi, commands[i].data,
						     commands[i].length);
		if (ret < 0)
			return ret;
	}
	dev_info(&ctx->dsi->dev, "AA551 timing sequence selected for %u Hz\n", ctx->refresh_rate);
	return 0;
}

static void panel_aa551_p_3_a0004_dsc_reset(struct panel_aa551_p_3_a0004_dsc *ctx)
{
	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	usleep_range(2000, 3000);
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	usleep_range(2000, 3000);
	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	usleep_range(2000, 3000);
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	msleep(25);
}

static int panel_aa551_p_3_a0004_dsc_on(struct panel_aa551_p_3_a0004_dsc *ctx)
{
	struct mipi_dsi_device *dsi = ctx->dsi;

	aa551_dcs_write_seq(dsi, 0xff, 0x08, 0x38, 0x00);
	aa551_dcs_write_seq(dsi, 0x35, 0x00);
	aa551_dcs_write_seq(dsi, 0x53, 0x20);
	//1-BIT ESD
	aa551_dcs_write_seq(dsi, 0xff, 0x08, 0x38, 0x06);
	aa551_dcs_write_seq(dsi, 0xc6, 0x01);
	aa551_dcs_write_seq(dsi, 0xff, 0x08, 0x38, 0x08);
	aa551_dcs_write_seq(dsi, 0xed, 0xff, 0xff, 0xff, 0xf7, 0xff, 0xff, 0xbf, 0xff);
	aa551_dcs_write_seq(dsi, 0xee, 0xfe, 0xef, 0xc1, 0xe0, 0x00, 0xc0, 0x01, 0x00);
	aa551_dcs_write_seq(dsi, 0xd2, 0x05);
	aa551_dcs_write_seq(dsi, 0xd3, 0x01);
	aa551_dcs_write_seq(dsi, 0xff, 0x08, 0x38, 0x02);
	//FUNCTION SELECT
	aa551_dcs_write_seq(dsi, 0xf8, 0x01);
	aa551_dcs_write_seq(dsi, 0xff, 0x08, 0x38, 0x20);
	//TE TEST MODE
	aa551_dcs_write_seq(dsi, 0xb3, 0x50);
	aa551_dcs_write_seq(dsi, 0xb5, 0x03);
	aa551_dcs_write_seq(dsi, 0xff, 0x08, 0x38, 0x08);
	aa551_dcs_write_seq(dsi, 0xc8, 0x62);
	aa551_dcs_write_seq(dsi, 0xff, 0x08, 0x38, 0x05);
	aa551_dcs_write_seq(dsi, 0x80, 0x19);
	//AOD EL INTERAL
	aa551_dcs_write_seq(dsi, 0xd0, 0xff, 0xaf, 0x56, 0x3d, 0x2d, 0x2d, 0x2d, 0x2d, 0xff);
	aa551_dcs_write_seq(dsi, 0xff, 0x08, 0x38, 0x00);
	aa551_dcs_write_seq(dsi, 0xff, 0x08, 0x38, 0x31);
	aa551_dcs_write_seq(dsi, 0xa0, 0xf3);
	//DSC 1.2 CONFIG
	//aa551_dcs_write_seq(dsi, 0xff, 0x08, 0x38, 0x07);
	//aa551_dcs_write_seq(dsi, 0x8a, 0x00);
	//aa551_dcs_write_seq(dsi, 0x8b, 0x11, 0xe0);
	//CHANGE PPS TABLE
	aa551_dcs_write_seq(dsi, 0x81,
			       0x00, 0x00, 0x00, 0x00, 0x00, 0x12, 0x00, 0x00,
			       0xab, 0x30, 0x80, 0x0a, 0xdc, 0x04, 0xf0, 0x00,
			       0x14, 0x02, 0x78, 0x02, 0x78, 0x02, 0x00, 0x02,
			       0x57, 0x00, 0x20, 0x01, 0xf8, 0x00, 0x08, 0x00,
			       0x0d, 0x05, 0x7a, 0x04, 0x4f, 0x18, 0x00, 0x10,
			       0xe0, 0x07, 0x10, 0x20, 0x00, 0x06, 0x0f, 0x0f,
			       0x33, 0x0e, 0x1c, 0x2a, 0x38, 0x46, 0x54, 0x62,
			       0x69, 0x70, 0x77, 0x79, 0x7b, 0x7d, 0x7e, 0x02,
			       0x02, 0x22, 0x00, 0x2a, 0x40, 0x2a, 0xbe, 0x3a,
			       0xfc, 0x3a, 0xfa, 0x3a, 0xf8, 0x3b, 0x38, 0x3b,
			       0x78, 0x3b, 0xb6, 0x4b, 0xb6, 0x4b, 0xf4, 0x4b,
			       0xf4, 0x6c, 0x34, 0x84, 0x74, 0x74, 0x00, 0x00,
			       0x00, 0x00, 0x00);
	//OSC 138.6MHZ
	aa551_dcs_write_seq(dsi, 0xff, 0x08, 0x38, 0x20);
	aa551_dcs_write_seq(dsi, 0xf2, 0x00);
	aa551_dcs_write_seq(dsi, 0xf5, 0x33);
	aa551_dcs_write_seq(dsi, 0xf6, 0xb7);
	aa551_dcs_write_seq(dsi, 0xf7, 0x98);
	aa551_dcs_write_seq(dsi, 0xf2, 0x01);
	//MIPI=556.8MHz_1113.6Mbps OSC
	aa551_dcs_write_seq(dsi, 0xff, 0x08, 0x38, 0x22);
	aa551_dcs_write_seq(dsi, 0xd0, 0x00, 0xd6, 0x11, 0x10, 0x32, 0x14, 0x14);
	aa551_dcs_write_seq(dsi, 0xd3, 0x00, 0xd6, 0x11, 0x10, 0x32, 0x14, 0x14);
	aa551_dcs_write_seq(dsi, 0xd6, 0x00, 0xd6, 0x11, 0x10, 0x32, 0x14, 0x14);
	aa551_dcs_write_seq(dsi, 0xd9, 0x00, 0xd6, 0x11, 0x10, 0x32, 0x14, 0x14);
	aa551_dcs_write_seq(dsi, 0xdc, 0x84, 0x94, 0xbb, 0x00, 0x20, 0xc4, 0xa1, 0xcc, 0x10);
	//TRIM_CMD enable
	aa551_dcs_write_seq(dsi, 0xdd, 0x01);
	aa551_dcs_write_seq(dsi, 0xde, 0xf1);
	aa551_dcs_write_seq(dsi, 0xdf, 0x80);
	aa551_dcs_write_seq(dsi, 0xe0, 0x02, 0x00, 0x5f, 0x21, 0x00, 0x3c, 0x28, 0x00); //P6_cmd key=3C
	//DCDC Setting
	aa551_dcs_write_seq(dsi, 0xff, 0x08, 0x38, 0x1f);
	aa551_dcs_write_seq(dsi, 0x83, 0xdb, 0x7f);
	aa551_dcs_write_seq(dsi, 0x84, 0x2d, 0x7f);
	aa551_dcs_write_seq(dsi, 0x85, 0x5f, 0x79, 0x07);
	//ESD fixed H linse
	aa551_dcs_write_seq(dsi, 0xff, 0x08, 0x38, 0x08);
	aa551_dcs_write_seq(dsi, 0xd2, 0x05);
	aa551_dcs_write_seq(dsi, 0xd3, 0x01);
	//72HZ--EM_DUTY 93.3
	aa551_dcs_write_seq(dsi, 0xff, 0x08, 0x38, 0x24);
	aa551_dcs_write_seq(dsi, 0x92, 0x8e);
	aa551_dcs_write_seq(dsi, 0xff, 0x08, 0x38, 0x23);
	aa551_dcs_write_seq(dsi, 0xcb, 0x01);
	aa551_dcs_write_seq(dsi, 0xff, 0x08, 0x38, 0x64);
	aa551_dcs_write_seq(dsi, 0x80, 0x00, 0x00, 0x4e, 0x05);
	aa551_dcs_write_seq(dsi, 0x81, 0x00, 0x00, 0x4e, 0x05);
	aa551_dcs_write_seq(dsi, 0x82, 0x00, 0x00, 0x4e, 0x05);
	aa551_dcs_write_seq(dsi, 0x83, 0x00, 0x00, 0x4e, 0x05);
	aa551_dcs_write_seq(dsi, 0x84, 0x00, 0x00, 0x4e, 0x05);
	aa551_dcs_write_seq(dsi, 0x85, 0x00, 0x00, 0x4e, 0x05);
	aa551_dcs_write_seq(dsi, 0x86, 0x00, 0x00, 0xcb, 0x05);
	aa551_dcs_write_seq(dsi, 0x87, 0x00, 0x02, 0xd7, 0x1e);
	aa551_dcs_write_seq(dsi, 0x88, 0x00, 0x03, 0x31, 0x1e);
	aa551_dcs_write_seq(dsi, 0x89, 0x00, 0x03, 0xb8, 0x1e);
	aa551_dcs_write_seq(dsi, 0x8a, 0x00, 0x04, 0x03, 0x1e);
	aa551_dcs_write_seq(dsi, 0x8b, 0x00, 0x04, 0x36, 0x1e);
	aa551_dcs_write_seq(dsi, 0x8c, 0x00, 0x04, 0xb0, 0x1e);
	aa551_dcs_write_seq(dsi, 0x8d, 0x00, 0x04, 0xb0, 0x1e);
	aa551_dcs_write_seq(dsi, 0x8e, 0x00, 0x04, 0xb0, 0x1e);
	aa551_dcs_write_seq(dsi, 0x8f, 0x00, 0x04, 0xb0, 0x1e);
	aa551_dcs_write_seq(dsi, 0x90, 0x00, 0x04, 0xb0, 0x1e);
	aa551_dcs_write_seq(dsi, 0x91, 0x00, 0x04, 0xb0, 0x1e);
	aa551_dcs_write_seq(dsi, 0x92, 0x00, 0x04, 0xb0, 0x1e);
	aa551_dcs_write_seq(dsi, 0x93, 0x00, 0x04, 0xb0, 0x1e);
	aa551_dcs_write_seq(dsi, 0x94, 0x00, 0x04, 0xb0, 0x1e);
	aa551_dcs_write_seq(dsi, 0x95, 0x00, 0x04, 0xb0, 0x1e);
	aa551_dcs_write_seq(dsi, 0x96, 0x00, 0x04, 0xb0, 0x1e);
	aa551_dcs_write_seq(dsi, 0x97, 0x00, 0x04, 0xb0, 0x1e);
	aa551_dcs_write_seq(dsi, 0x98, 0x00, 0x04, 0xb0, 0x1e);
	aa551_dcs_write_seq(dsi, 0xff, 0x08, 0x38, 0x23);
	aa551_dcs_write_seq(dsi, 0xcb, 0x00);
	//HRST
	aa551_dcs_write_seq(dsi, 0xff, 0x08, 0x38, 0x08);
	aa551_dcs_write_seq(dsi, 0xc8, 0x62);
	aa551_dcs_write_seq(dsi, 0xff, 0x08, 0x38, 0x01);
	aa551_dcs_write_seq(dsi, 0x82, 0x00, 0x00, 0x40, 0x0a, 0x00, 0x11, 0x08, 0x04, 0x00, 0x0f, 0x00, 0x00, 0x3e, 0xfc);
	aa551_dcs_write_seq(dsi, 0x83, 0x00, 0x00, 0xc0, 0x00, 0x10, 0x18, 0x08, 0x18, 0x00, 0x00, 0x00, 0x00, 0x3e, 0xfc);
	aa551_dcs_write_seq(dsi, 0x94, 0x09);
	aa551_dcs_write_seq(dsi, 0x98, 0x08);
	aa551_dcs_write_seq(dsi, 0xff, 0x08, 0x38, 0x09);
	aa551_dcs_write_seq(dsi, 0x8e, 0x08, 0x10);
	aa551_dcs_write_seq(dsi, 0xb4, 0x04);
	aa551_dcs_write_seq(dsi, 0xee, 0x40);
	aa551_dcs_write_seq(dsi, 0xff, 0x08, 0x38, 0x0a);
	aa551_dcs_write_seq(dsi, 0x82, 0x08, 0x08, 0x00, 0x40);
	aa551_dcs_write_seq(dsi, 0x83, 0x08, 0x18, 0x00, 0xc0);
	aa551_dcs_write_seq(dsi, 0x90, 0x08, 0x08, 0x00, 0x40);
	aa551_dcs_write_seq(dsi, 0x91, 0x08, 0x18, 0x00, 0xc0);
	aa551_dcs_write_seq(dsi, 0x9e, 0x20, 0x20, 0x00, 0xc0);
	aa551_dcs_write_seq(dsi, 0x9f, 0x20, 0x20, 0x00, 0xc0);
	aa551_dcs_write_seq(dsi, 0xff, 0x08, 0x38, 0x0b);
	aa551_dcs_write_seq(dsi, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
	aa551_dcs_write_seq(dsi, 0x82, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
	aa551_dcs_write_seq(dsi, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
	aa551_dcs_write_seq(dsi, 0x86, 0x00);
	aa551_dcs_write_seq(dsi, 0x87, 0x00);
	aa551_dcs_write_seq(dsi, 0x88, 0x00);
	aa551_dcs_write_seq(dsi, 0x89, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
	aa551_dcs_write_seq(dsi, 0x8a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
	aa551_dcs_write_seq(dsi, 0x8b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
	aa551_dcs_write_seq(dsi, 0x81, 0x08, 0x08, 0x08, 0x08, 0x18, 0x18, 0x18, 0x18);
	aa551_dcs_write_seq(dsi, 0x83, 0x08, 0x08, 0x08, 0x08, 0x18, 0x18, 0x18, 0x18);
	aa551_dcs_write_seq(dsi, 0x85, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20);
	aa551_dcs_write_seq(dsi, 0x8c, 0x10);
	aa551_dcs_write_seq(dsi, 0x8d, 0x03);
	aa551_dcs_write_seq(dsi, 0x8e, 0xbf);
	aa551_dcs_write_seq(dsi, 0x8f, 0x33, 0xbf, 0xbf, 0x33, 0xbf, 0xbf, 0x03, 0xbf);
	aa551_dcs_write_seq(dsi, 0x90, 0x00, 0x05, 0x05, 0x00, 0x05, 0x05, 0x00, 0x05);
	aa551_dcs_write_seq(dsi, 0x91, 0x08, 0x08, 0x08, 0x08, 0x18, 0x18, 0x18, 0x18);
	aa551_dcs_write_seq(dsi, 0x93, 0x08, 0x00, 0x00, 0x00, 0x10);
	aa551_dcs_write_seq(dsi, 0xff, 0x08, 0x38, 0x0d);
	aa551_dcs_write_seq(dsi, 0xcf, 0x84);
	aa551_dcs_write_seq(dsi, 0xd0, 0x40);
	aa551_dcs_write_seq(dsi, 0xd1, 0x00);
	aa551_dcs_write_seq(dsi, 0xd2, 0x00);
	aa551_dcs_write_seq(dsi, 0xd3, 0x00);
	aa551_dcs_write_seq(dsi, 0xd4, 0x00);
	aa551_dcs_write_seq(dsi, 0xd5, 0x17);
	aa551_dcs_write_seq(dsi, 0xd6, 0x17);
	aa551_dcs_write_seq(dsi, 0xd7, 0x00);
	aa551_dcs_write_seq(dsi, 0xd8, 0x00);
	//min fps
	aa551_dcs_write_seq(dsi, 0xff, 0x08, 0x38, 0x02);
	aa551_dcs_write_seq(dsi, 0xa6, 0x00);
	aa551_dcs_write_seq(dsi, 0xa7, 0x40);
	aa551_dcs_write_seq(dsi, 0xa2, 0x20);
	aa551_dcs_write_seq(dsi, 0xa0, 0x07, 0x0b, 0x63, 0x00, 0x00);
	aa551_dcs_write_seq(dsi, 0xb0, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00);
	aa551_dcs_write_seq(dsi, 0xb1, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00);
	aa551_dcs_write_seq(dsi, 0xb6, 0x80);
	//TE
	aa551_dcs_write_seq(dsi, 0xff, 0x08, 0x38, 0x20);
	aa551_dcs_write_seq(dsi, 0xbc, 0x32);
	aa551_dcs_write_seq(dsi, 0xbd, 0x34);
	aa551_dcs_write_seq(dsi, 0xbe, 0x56);
	aa551_dcs_write_seq(dsi, 0xff, 0x08, 0x38, 0x4f);
	aa551_dcs_write_seq(dsi, 0x81, 0x02);
	aa551_dcs_write_seq(dsi, 0x80, 0x01);
	aa551_dcs_write_seq(dsi, 0xff, 0x08, 0x38, 0x2d);
	aa551_dcs_write_seq(dsi, 0x81, 0x02);
	aa551_dcs_write_seq(dsi, 0xce, 0x52);
	aa551_dcs_write_seq(dsi, 0xff, 0x08, 0x38, 0x1f);
	aa551_dcs_write_seq(dsi, 0xaa, 0x04);
	//Vint23
	aa551_dcs_write_seq(dsi, 0xff, 0x08, 0x38, 0x4e);
	aa551_dcs_write_seq(dsi, 0xd4, 0x00, 0x06, 0x06, 0x06, 0x06, 0x06, 0x00, 0x80);
	aa551_dcs_write_seq(dsi, 0xd1, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80);
	//120HZ
	aa551_dcs_write_seq(dsi, 0xff, 0x08, 0x38, 0x2d);
	aa551_dcs_write_seq(dsi, 0x80, 0x00);
	aa551_dcs_write_seq(dsi, 0xd0, 0x01);
	//VINT2 DELAY
	aa551_dcs_write_seq(dsi, 0xff, 0x08, 0x38, 0x4e);
	aa551_dcs_write_seq(dsi, 0xb1, 0x04);
	//Peaklum 1023
	aa551_dcs_write_seq(dsi, 0xff, 0x08, 0x38, 0x63);
	aa551_dcs_write_seq(dsi, 0xa0, 0x81);
	aa551_dcs_write_seq(dsi, 0xff, 0x08, 0x38, 0x00);
	//ODC
	aa551_dcs_write_seq(dsi, 0xff, 0x08, 0x38, 0x49);
	aa551_dcs_write_seq(dsi, 0x98, 0x08, 0x00, 0x8c, 0x02);
	aa551_dcs_write_seq(dsi, 0xa0, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04, 0x05, 0x04, 0x04, 0x03, 0x04);
	aa551_dcs_write_seq(dsi, 0xa4, 0x04, 0x01, 0x01, 0x00, 0x01, 0x00, 0x04, 0x04, 0x00, 0x04, 0x04, 0x04, 0x05, 0x04, 0x04, 0x04, 0x04);
	aa551_dcs_write_seq(dsi, 0xa5, 0x04, 0x04, 0x04, 0x04, 0xfa, 0x0f, 0x10, 0x04, 0x04, 0x00, 0x04, 0x09, 0x05, 0x04, 0x03, 0x04);
	aa551_dcs_write_seq(dsi, 0xa9, 0x00, 0x01, 0x01, 0x00, 0x01, 0x00, 0x04, 0x04, 0x00, 0x03, 0x04, 0x04, 0x05, 0x04, 0x04, 0x04, 0x04);
	aa551_dcs_write_seq(dsi, 0xaa, 0x04, 0x04, 0x04, 0x04, 0x08, 0x08, 0x04, 0x04, 0x04, 0x00, 0x04, 0x05, 0x04, 0x04, 0x04, 0x04);
	aa551_dcs_write_seq(dsi, 0xae, 0x02, 0x01, 0x01, 0x00, 0x01, 0x00, 0x04, 0x04, 0x00, 0x03, 0x04, 0x04, 0x05, 0x04, 0x04, 0x04, 0x04);
	aa551_dcs_write_seq(dsi, 0xb0, 0x01, 0x01, 0x01, 0x01, 0x0b, 0x01, 0x16, 0x00, 0x05, 0x03, 0x35, 0x03, 0x15, 0x0f, 0x05, 0x20, 0x10, 0x23);
	aa551_dcs_write_seq(dsi, 0xb1, 0x00, 0x00, 0x00, 0x01, 0x05, 0xfe, 0xfe, 0x05, 0x01, 0x01, 0x69, 0xfe, 0x37, 0x30, 0x24, 0x16, 0x08, 0x03);
	aa551_dcs_write_seq(dsi, 0xb4, 0x03, 0x06, 0x06, 0x06, 0x06, 0x06, 0x14, 0x06, 0x05, 0x01, 0x24, 0x02, 0x13, 0x0e, 0x06, 0x20, 0x0f, 0x22);
	aa551_dcs_write_seq(dsi, 0xb5, 0x1d, 0x13, 0x04, 0xff, 0x10, 0x0f, 0x0f, 0x0c, 0x02, 0x03, 0x74, 0x3c, 0x38, 0x2f, 0x26, 0x15, 0x0a, 0x04);
	aa551_dcs_write_seq(dsi, 0xb8, 0x04, 0x04, 0x02, 0x08, 0x02, 0x01, 0x18, 0x07, 0x05, 0x03, 0x34, 0x16, 0x13, 0x0f, 0x08, 0x55, 0x28, 0x24);
	aa551_dcs_write_seq(dsi, 0xb9, 0x1d, 0x13, 0x07, 0x04, 0x30, 0x2b, 0x24, 0x05, 0x02, 0x01, 0x70, 0x3b, 0x33, 0x2b, 0x1f, 0x10, 0x05, 0x01);
	//ODC POWER ON
	aa551_dcs_write_seq(dsi, 0xff, 0x08, 0x38, 0x49);
	aa551_dcs_write_seq(dsi, 0xdc, 0x03);
	//VDDI 0.979V
	aa551_dcs_write_seq(dsi, 0xff, 0x08, 0x38, 0x05);
	aa551_dcs_write_seq(dsi, 0x95, 0x66);
	//PHBM
	aa551_dcs_write_seq(dsi, 0xff, 0x08, 0x38, 0x1e);
	aa551_dcs_write_seq(dsi, 0xc0, 0x94);
	aa551_dcs_write_seq(dsi, 0xc1, 0x20);
	aa551_dcs_write_seq(dsi, 0xc4, 0x04, 0x40);
	aa551_dcs_write_seq(dsi, 0xc9, 0x40);
	aa551_dcs_write_seq(dsi, 0xc2, 0xff, 0xff, 0xed, 0x61, 0x00, 0x62);
	aa551_dcs_write_seq(dsi, 0xc5,
			       0x44, 0x00, 0x00, 0x44, 0x00, 0x00, 0x44, 0x00,
			       0x00, 0x43, 0x07, 0x73, 0x21, 0x75, 0xa7, 0x00,
			       0xc3, 0x00, 0x44, 0x00, 0x00, 0x44, 0x00, 0x00,
			       0x44, 0x00, 0x00, 0x44, 0x00, 0x00, 0x44, 0x00,
			       0x00, 0x44, 0x00, 0x00, 0x04, 0x00);
	aa551_dcs_write_seq(dsi, 0xff, 0x08, 0x38, 0x13);
	aa551_dcs_write_seq(dsi, 0xd1, 0x88);
	aa551_dcs_write_seq(dsi, 0xff, 0x08, 0x38, 0x17);
	aa551_dcs_write_seq(dsi, 0xa0, 0xcc);
	aa551_dcs_write_seq(dsi, 0xae, 0x00);
	aa551_dcs_write_seq(dsi, 0xff, 0x08, 0x38, 0x24);
	aa551_dcs_write_seq(dsi, 0x80, 0x04);
	aa551_dcs_write_seq(dsi, 0x81, 0x40);
	aa551_dcs_write_seq(dsi, 0xff, 0x08, 0x38, 0x10);
	aa551_dcs_write_seq(dsi, 0x83, 0x2b);
	aa551_dcs_write_seq(dsi, 0xff, 0x08, 0x38, 0x1f);
	aa551_dcs_write_seq(dsi, 0x80, 0x08);
	aa551_dcs_write_seq(dsi, 0xff, 0x08, 0x38, 0x00);
	aa551_dcs_write_seq(dsi, 0x96, 0x07);
	aa551_dcs_write_seq(dsi, 0x97, 0xe9);
	aa551_dcs_write_seq(dsi, 0x98, 0x02);
	aa551_dcs_write_seq(dsi, 0x99, 0x78);
	aa551_dcs_write_seq(dsi, 0xff, 0x08, 0x38, 0x08);
	aa551_dcs_write_seq(dsi, 0xc8, 0x62);
	aa551_dcs_write_seq(dsi, 0xff, 0x08, 0x38, 0x00);

	return 0;
}

static int panel_aa551_p_3_a0004_dsc_off(struct panel_aa551_p_3_a0004_dsc *ctx)
{
	struct mipi_dsi_device *dsi = ctx->dsi;
	struct device *dev = &dsi->dev;
	int ret;

	//mdss-dsi-loading-effect-off-command
	aa551_dcs_write_seq(dsi, 0xff, 0x08, 0x38, 0x63);
	aa551_dcs_write_seq(dsi, 0x95, 0xfc, 0xfc, 0xeb, 0xda, 0xc9, 0xb9, 0xa8, 0x97, 0x86, 0x75, 0x64, 0x54, 0x43, 0x32, 0x21, 0x10);
	aa551_dcs_write_seq(dsi, 0x96, 0xff, 0x88, 0x90, 0x98, 0xa0, 0xff, 0xa8, 0xb0, 0xb8, 0xc0, 0xff, 0xc7, 0xcf, 0xd7, 0xdf, 0xff, 0xe7, 0xef, 0xf7, 0xff);
	aa551_dcs_write_seq(dsi, 0x97, 0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x99, 0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0x00);
	aa551_dcs_write_seq(dsi, 0x90, 0x01);
	aa551_dcs_write_seq(dsi, 0xff, 0x08, 0x38, 0x00);

	msleep(120);

	ret = mipi_dsi_dcs_set_display_off(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to set display off: %d\n", ret);
		return ret;
	}
	msleep(20);

	ret = mipi_dsi_dcs_enter_sleep_mode(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to enter sleep mode: %d\n", ret);
		return ret;
	}
	msleep(120);

	return 0;
}

static int panel_aa551_p_3_a0004_dsc_turn_on(struct panel_aa551_p_3_a0004_dsc *ctx)
{
	struct mipi_dsi_device *dsi = ctx->dsi;

	mipi_dsi_dcs_exit_sleep_mode(dsi);

	msleep(120); // Required



	mipi_dsi_dcs_set_display_on(dsi);

	return 0;
}

static int panel_aa551_p_3_a0004_dsc_prepare(struct drm_panel *panel)
{
	struct panel_aa551_p_3_a0004_dsc *ctx = to_panel_aa551_p_3_a0004_dsc(panel);
	struct device *dev = &ctx->dsi->dev;
	struct drm_dsc_picture_parameter_set pps;
	int ret;

	ret = regulator_enable(ctx->vddio);
	if (ret) {
		dev_err(dev, "failed to enable vddio regulator: %d\n", ret);
		return ret;
	}

	ret = regulator_enable(ctx->vci);
	if (ret) {
		dev_err(dev, "failed to enable vci regulator: %d\n", ret);
		return ret;
	}

	panel_aa551_p_3_a0004_dsc_reset(ctx);

	ret = panel_aa551_p_3_a0004_dsc_on(ctx);
	if (ret < 0) {
		regulator_disable(ctx->vci);
		regulator_disable(ctx->vddio);
		dev_err(dev, "Failed to initialize panel: %d\n", ret);
		gpiod_set_value_cansleep(ctx->reset_gpio, 1);
		return ret;
	}

	ret = aa551_set_refresh_rate(ctx);
	if (ret < 0) {
		gpiod_set_value_cansleep(ctx->reset_gpio, 1);
		regulator_disable(ctx->vci);
		regulator_disable(ctx->vddio);
		return ret;
	}
	aa551_dcs_write_seq(ctx->dsi, 0x2c);

	drm_dsc_pps_payload_pack(&pps, &ctx->dsc);

	ret = mipi_dsi_picture_parameter_set(ctx->dsi, &pps);
	if (ret < 0) {
		dev_err(panel->dev, "failed to transmit PPS: %d\n", ret);
		return ret;
	}

	ret = mipi_dsi_compression_mode(ctx->dsi, true);
	if (ret < 0) {
		dev_err(dev, "failed to enable compression mode: %d\n", ret);
		return ret;
	}

	msleep(28); /* TODO: Is this panel-dependent? */

	return 0;
}

static int panel_aa551_p_3_a0004_dsc_enable(struct drm_panel *panel)
{
	struct panel_aa551_p_3_a0004_dsc *ctx = to_panel_aa551_p_3_a0004_dsc(panel);
	struct device *dev = &ctx->dsi->dev;
	int ret;

	ret = panel_aa551_p_3_a0004_dsc_turn_on(ctx);
	if (ret < 0)
		dev_err(dev, "Failed to enable panel: %d\n", ret);

	return 0;
}

static int panel_aa551_p_3_a0004_dsc_disable(struct drm_panel *panel)
{
	struct panel_aa551_p_3_a0004_dsc *ctx = to_panel_aa551_p_3_a0004_dsc(panel);
	struct device *dev = &ctx->dsi->dev;
	int ret;

	ret = panel_aa551_p_3_a0004_dsc_off(ctx);
	if (ret < 0)
		dev_err(dev, "Failed to un-initialize panel: %d\n", ret);

	return 0;
}

static int panel_aa551_p_3_a0004_dsc_unprepare(struct drm_panel *panel)
{
	struct panel_aa551_p_3_a0004_dsc *ctx = to_panel_aa551_p_3_a0004_dsc(panel);
	struct device *dev = &ctx->dsi->dev;
	int ret;

	ret = panel_aa551_p_3_a0004_dsc_off(ctx);
	if (ret < 0)
		dev_err(dev, "Failed to un-initialize panel: %d\n", ret);

	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	regulator_disable(ctx->vci);
	regulator_disable(ctx->vddio);

	return 0;
}

const struct drm_display_mode panel_aa551_p_3_a0004_dsc_modes[] = {
	{
		.clock = (1264 + 26 + 2 + 26) * (2780 + 22 + 2 + 42) * 165 / 1000,
		.hdisplay = 1264,
		.hsync_start = 1264 + 26,
		.hsync_end = 1264 + 26 + 2,
		.htotal = 1264 + 26 + 2 + 26,
		.vdisplay = 2780,
		.vsync_start = 2780 + 22,
		.vsync_end = 2780 + 22 + 2,
		.vtotal = 2780 + 22 + 2 + 42,
	},
	{
		.clock = (1264 + 26 + 2 + 26) * (2780 + 22 + 2 + 42) * 120 / 1000,
		.hdisplay = 1264,
		.hsync_start = 1264 + 26,
		.hsync_end = 1264 + 26 + 2,
		.htotal = 1264 + 26 + 2 + 26,
		.vdisplay = 2780,
		.vsync_start = 2780 + 22,
		.vsync_end = 2780 + 22 + 2,
		.vtotal = 2780 + 22 + 2 + 42,
	},
	{
		.clock = (1264 + 26 + 2 + 26) * (2780 + 971 + 2 + 42) * 90 / 1000,
		.hdisplay = 1264,
		.hsync_start = 1264 + 26,
		.hsync_end = 1264 + 26 + 2,
		.htotal = 1264 + 26 + 2 + 26,
		.vdisplay = 2780,
		.vsync_start = 2780 + 971,
		.vsync_end = 2780 + 971 + 2,
		.vtotal = 2780 + 971 + 2 + 42,
	},
	{
		.clock = (1264 + 26 + 2 + 26) * (2780 + 2868 + 2 + 42) * 60 / 1000,
		.hdisplay = 1264,
		.hsync_start = 1264 + 26,
		.hsync_end = 1264 + 26 + 2,
		.htotal = 1264 + 26 + 2 + 26,
		.vdisplay = 2780,
		.vsync_start = 2780 + 2868,
		.vsync_end = 2780 + 2868 + 2,
		.vtotal = 2780 + 2868 + 2 + 42,
	},
	{
		.clock = (1264 + 26 + 2 + 26) * (2780 + 8560 + 2 + 42) * 30 / 1000,
		.hdisplay = 1264,
		.hsync_start = 1264 + 26,
		.hsync_end = 1264 + 26 + 2,
		.htotal = 1264 + 26 + 2 + 26,
		.vdisplay = 2780,
		.vsync_start = 2780 + 8560,
		.vsync_end = 2780 + 8560 + 2,
		.vtotal = 2780 + 8560 + 2 + 42,
	},
};

static int panel_aa551_p_3_a0004_dsc_get_modes(struct drm_panel *panel,
					       struct drm_connector *connector)
{
	struct drm_display_mode *mode;
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(panel_aa551_p_3_a0004_dsc_modes); i++) {
		mode = drm_mode_duplicate(connector->dev, &panel_aa551_p_3_a0004_dsc_modes[i]);
		if (!mode)
			return -ENOMEM;

		drm_mode_set_name(mode);

		mode->type = DRM_MODE_TYPE_DRIVER;
		/* Use the verified mode for fbcon and fresh desktop configurations. */
		if (drm_mode_vrefresh(mode) == 60)
			mode->type |= DRM_MODE_TYPE_PREFERRED;

		drm_mode_probed_add(connector, mode);
	}

	return ARRAY_SIZE(panel_aa551_p_3_a0004_dsc_modes);
}

static const struct drm_panel_funcs panel_aa551_p_3_a0004_dsc_panel_funcs = {
	.prepare = panel_aa551_p_3_a0004_dsc_prepare,
	.unprepare = panel_aa551_p_3_a0004_dsc_unprepare,
	.enable = panel_aa551_p_3_a0004_dsc_enable,
	.disable = panel_aa551_p_3_a0004_dsc_disable,
	.get_modes = panel_aa551_p_3_a0004_dsc_get_modes,
};

static int panel_aa551_p_3_a0004_dsc_bl_update_status(struct backlight_device *bl)
{
	struct mipi_dsi_device *dsi = bl_get_data(bl);
	u16 brightness = backlight_get_brightness(bl);
	int ret;

	aa551_dcs_write_seq(dsi, 0xff, 0x08, 0x38, 0x4e);
	if (brightness >= 1088 && brightness <= 1768) {
			aa551_dcs_write_seq(dsi, 0xd4, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80);
			aa551_dcs_write_seq(dsi, 0xd1, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80);
	} else {
			aa551_dcs_write_seq(dsi, 0xd4, 0x00, 0x06, 0x06, 0x06, 0x06, 0x06, 0x00, 0x80);
			aa551_dcs_write_seq(dsi, 0xd1, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x80);
	}
	aa551_dcs_write_seq(dsi, 0xff, 0x08, 0x38, 0x00);

	ret = mipi_dsi_dcs_set_display_brightness_large(dsi, brightness);
	if (ret < 0)
			return ret;

	aa551_dcs_write_seq(dsi, 0xff, 0x08, 0x38, 0x1e);
	aa551_dcs_write_seq(dsi, 0xc2, 0xff, 0xff, 0xe5, 0x61, 0x00, 0x62);
	aa551_dcs_write_seq(dsi, 0xff, 0x08, 0x38, 0x00);

	return 0;
}

static const struct backlight_ops panel_aa551_p_3_a0004_dsc_bl_ops = {
	.update_status = panel_aa551_p_3_a0004_dsc_bl_update_status,
};

static struct backlight_device *
panel_aa551_p_3_a0004_dsc_create_backlight(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	const struct backlight_properties props = {
		.type = BACKLIGHT_RAW,
		.brightness = 1433,
		.max_brightness = 4094,
	};

	return devm_backlight_device_register(dev, dev_name(dev), dev, dsi,
					      &panel_aa551_p_3_a0004_dsc_bl_ops, &props);
}

static int panel_aa551_p_3_a0004_dsc_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct panel_aa551_p_3_a0004_dsc *ctx;
	struct aa551_mode_bridge *mode_bridge;
	int ret;

	ctx = devm_drm_panel_alloc(dev, struct panel_aa551_p_3_a0004_dsc,
				   panel, &panel_aa551_p_3_a0004_dsc_panel_funcs,
				   DRM_MODE_CONNECTOR_DSI);
	if (IS_ERR(ctx))
		return PTR_ERR(ctx);

	ctx->vddio = devm_regulator_get(dev, "vddio");
	if (IS_ERR(ctx->vddio))
		return dev_err_probe(dev, PTR_ERR(ctx->vddio), "failed to get vddio regulator\n");

	ctx->vci = devm_regulator_get(dev, "vci");
	if (IS_ERR(ctx->vci))
		return dev_err_probe(dev, PTR_ERR(ctx->vci), "failed to get vci regulator\n");

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio))
		return dev_err_probe(dev, PTR_ERR(ctx->reset_gpio),
				     "Failed to get reset-gpios\n");

	ctx->dsi = dsi;
	mipi_dsi_set_drvdata(dsi, ctx);

	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB101010;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO_BURST |
			  MIPI_DSI_MODE_NO_EOT_PACKET |
			  MIPI_DSI_CLOCK_NON_CONTINUOUS;

	ctx->panel.prepare_prev_first = true;

	ret = drm_panel_of_backlight(&ctx->panel);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to get backlight\n");

	/* Fallback to DCS backlight if no backlight is defined in DT */
	if (!ctx->panel.backlight) {
		ctx->panel.backlight = panel_aa551_p_3_a0004_dsc_create_backlight(dsi);
		if (IS_ERR(ctx->panel.backlight))
			return dev_err_probe(dev, PTR_ERR(ctx->panel.backlight),
					     "Failed to create backlight\n");
	}

	ctx->refresh_rate = 60;
	mode_bridge = devm_drm_bridge_alloc(dev, struct aa551_mode_bridge,
					  bridge, &aa551_mode_bridge_funcs);
	if (IS_ERR(mode_bridge))
		return PTR_ERR(mode_bridge);
	mode_bridge->panel = ctx;
	mode_bridge->panel_bridge = devm_drm_panel_bridge_add(dev, &ctx->panel);
	if (IS_ERR(mode_bridge->panel_bridge))
		return PTR_ERR(mode_bridge->panel_bridge);
	/* Only the mode bridge is discoverable through the DSI endpoint. */
	mode_bridge->panel_bridge->of_node = NULL;
	mode_bridge->bridge.of_node = dev->of_node;
	mode_bridge->bridge.type = DRM_MODE_CONNECTOR_DSI;
	mode_bridge->bridge.pre_enable_prev_first = true;
	ret = devm_drm_bridge_add(dev, &mode_bridge->bridge);
	if (ret)
		return ret;

	/* This panel only supports DSC; unconditionally enable it */
	dsi->dsc = &ctx->dsc;

	ctx->dsc.dsc_version_major = 1;
	ctx->dsc.dsc_version_minor = 2;

	ctx->dsc.slice_height = 20; //works 20
	ctx->dsc.slice_width = 632; //works 632
	ctx->dsi->dsc_slice_per_pkt = 1; /* Verified with the current mainline pipeline. */
	ctx->dsc.slice_count = 2; //works 2
	ctx->dsc.convert_rgb = true; //works true
	ctx->dsc.bits_per_component = 10; //works 10
	ctx->dsc.bits_per_pixel = 8 << 4; /* 4 fractional bits */
	//ctx->dsc.block_pred_enable = true;

	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		return dev_err_probe(dev, ret, "Failed to attach to DSI host\n");
	}

	return 0;
}

static void panel_aa551_p_3_a0004_dsc_remove(struct mipi_dsi_device *dsi)
{
	int ret;

	ret = mipi_dsi_detach(dsi);
	if (ret < 0)
		dev_err(&dsi->dev, "Failed to detach from DSI host: %d\n", ret);

}

static const struct of_device_id panel_aa551_p_3_a0004_dsc_of_match[] = {
	{
			.compatible = "panel,aa551-p-3-a0004-dsc",
	},
	{},
};
MODULE_DEVICE_TABLE(of, panel_aa551_p_3_a0004_dsc_of_match);

static struct mipi_dsi_driver panel_aa551_p_3_a0004_dsc_driver = {
	.probe = panel_aa551_p_3_a0004_dsc_probe,
	.remove = panel_aa551_p_3_a0004_dsc_remove,
	.driver = {
		.name = "panel-panel-aa551-p-3-a0004-dsc",
		.of_match_table = panel_aa551_p_3_a0004_dsc_of_match,
	},
};
module_mipi_dsi_driver(panel_aa551_p_3_a0004_dsc_driver);

MODULE_AUTHOR("linux-mdss-dsi-panel-driver-generator <fix@me>"); // FIXME
MODULE_DESCRIPTION("DRM driver for AA551 P 3 A0004 dsc cmd mode panel");
MODULE_LICENSE("GPL");
