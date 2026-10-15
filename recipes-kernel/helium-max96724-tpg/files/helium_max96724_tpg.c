// SPDX-License-Identifier: GPL-2.0-only
/*
 * helium_max96724_tpg.c - MAX96724 video pattern generator as a tegracam
 * sensor, for testing the PCB Arts Helium GMSL2 -> MIPI CSI -> VI path
 * without a camera.
 *
 * The register sequence follows the upstream staging driver
 * drivers/staging/media/max96712/max96712.c (CSI 2x4 D-PHY setup, VTG/VPG)
 * plus what the staging driver relies on board defaults for, taken from
 * Analog Devices' maxim-serdes max96724.c (patch series "media: i2c: add
 * Maxim GMSL2/3 serializer and deserializer drivers"):
 *  - the pattern is injected into video pipe 0, and in 2x4 mode PHY1 is the
 *    master of port A, so pipe 0 must be sent to PHY1 (MIPI_CTRL_SEL) with
 *    explicit VC/DT maps for the pixels and FS/FE; with the power-on
 *    defaults nothing leaves the deserializer;
 *  - 75 MHz pattern clock (DEBUG_EXTRA) and CEA 1080p30 timings;
 *  - DPLL soft reset around the CSI rate change.
 */

#include <nvidia/conftest.h>

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/regmap.h>

#include <media/tegra_v4l2_camera.h>
#include <media/tegracam_core.h>

#define MAX96724_REG_DEV_ID		0x000d
#define MAX96724_DEBUG_EXTRA		0x0009
#define MAX96724_DEBUG_EXTRA_PCLK_75MHZ	0x01
#define MAX96724_MIPI_CTRL_SEL		0x08ca
#define MAX96724_DPLL_0(phy)		(0x1c00 + (phy) * 0x100)

/* MIPI_TX registers of pipe 0. */
#define MAX96724_MIPI_TX11_P0		0x090b
#define MAX96724_MIPI_TX12_P0		0x090c
#define MAX96724_MIPI_TX13_P0(m)	(0x090d + (m) * 2)
#define MAX96724_MIPI_TX14_P0(m)	(0x090e + (m) * 2)
#define MAX96724_MIPI_TX45_P0		0x092d

#define MIPI_DT_FS			0x00
#define MIPI_DT_FE			0x01
#define MIPI_DT_RGB888			0x24
#define MAX96712_DEV_ID			0xa0
#define MAX96724_DEV_ID			0xa2

/* Upstream: MAX96724 D-PHY rate is 1200 Mbps/lane. */
static int dpll_mbps = 1200;
module_param(dpll_mbps, int, 0644);
MODULE_PARM_DESC(dpll_mbps, "MIPI D-PHY rate per lane in Mbps (default 1200)");
#define MAX96724_DPLL_MBPS		dpll_mbps

static int pattern;
module_param(pattern, int, 0644);
MODULE_PARM_DESC(pattern, "0 = checkerboard (default), 1 = gradient");

static int csi_port = 1;
module_param(csi_port, int, 0644);
MODULE_PARM_DESC(csi_port, "Deserializer 4-lane port: 0 = A (PHY0/1), 1 = B (PHY2/3, default)");

static int quad_2lane = 1;
module_param(quad_2lane, int, 0644);
MODULE_PARM_DESC(quad_2lane, "1 = 4x2 mode, pattern on all four PHYs, 2 lanes each (default; the 2x4 path still gets payload CRC errors on Tegra CSI C+D)");

static int lane_map = 0xe4;
module_param(lane_map, int, 0644);
MODULE_PARM_DESC(lane_map, "MIPI lane mapping of the 4-lane port (0x08a3/0x08a4)");

static int lane_polarity;
module_param(lane_polarity, int, 0644);
MODULE_PARM_DESC(lane_polarity, "MIPI PHY0/1 polarity bits written to 0x08a5");

static const struct of_device_id max96724_tpg_of_match[] = {
	{ .compatible = "pcbarts,max96724-tpg", },
	{ },
};
MODULE_DEVICE_TABLE(of, max96724_tpg_of_match);

static const u32 ctrl_cid_list[] = {
	TEGRA_CAMERA_CID_GAIN,
	TEGRA_CAMERA_CID_EXPOSURE,
	TEGRA_CAMERA_CID_FRAME_RATE,
	TEGRA_CAMERA_CID_SENSOR_MODE_ID,
};

static const int max96724_tpg_30fps[] = {
	30,
};

/* Must match mode0 in the device tree. */
static const struct camera_common_frmfmt max96724_tpg_frmfmt[] = {
	{{1920, 1080}, max96724_tpg_30fps, 1, 0, 0},
};

struct max96724_tpg {
	struct i2c_client		*i2c_client;
	struct regmap			*regmap;
	struct tegracam_device		*tc_dev;
};

static const struct regmap_config max96724_tpg_regmap_config = {
	.reg_bits = 16,
	.val_bits = 8,
	.max_register = 0x1f00,
	.use_single_read = true,
	.use_single_write = true,
};

static int max96724_write(struct max96724_tpg *priv, unsigned int reg, u8 val)
{
	int err = regmap_write(priv->regmap, reg, val);

	if (err)
		dev_err(&priv->i2c_client->dev, "write 0x%04x failed\n", reg);

	return err;
}

static int max96724_update_bits(struct max96724_tpg *priv, unsigned int reg,
				u8 mask, u8 val)
{
	int err = regmap_update_bits(priv->regmap, reg, mask, val);

	if (err)
		dev_err(&priv->i2c_client->dev, "update 0x%04x failed\n", reg);

	return err;
}

/* Write val MSB first over count consecutive registers. */
static int max96724_write_be(struct max96724_tpg *priv, unsigned int reg,
			     u32 val, unsigned int count)
{
	unsigned int i;
	int err = 0;

	for (i = 0; i < count && !err; i++)
		err = max96724_write(priv, reg + i,
				     (val >> ((count - 1 - i) * 8)) & 0xff);

	return err;
}

static void max96724_mipi_enable(struct max96724_tpg *priv, bool enable)
{
	if (enable) {
		max96724_update_bits(priv, 0x040b, 0x02, 0x02);
		max96724_update_bits(priv, 0x08a0, 0x80, 0x80);
	} else {
		max96724_update_bits(priv, 0x08a0, 0x80, 0x00);
		max96724_update_bits(priv, 0x040b, 0x02, 0x00);
	}
}

/*
 * 4x2 mode: each PHY is a 2-lane port with its own clock and pipe N goes to
 * PHY N, so the pattern (which reaches every pipe) leaves on all four PHYs.
 * Used to find which Tegra CSI brick each deserializer PHY is wired to.
 */
static void max96724_mipi_configure_4x2(struct max96724_tpg *priv)
{
	static const u8 dts[] = { MIPI_DT_RGB888, MIPI_DT_FS, MIPI_DT_FE };
	const u8 dpll = ((MAX96724_DPLL_MBPS / 100) & 0x1f) | BIT(5);
	unsigned int phy, i, base;

	max96724_write(priv, 0x08a0, 0x01);
	max96724_write(priv, 0x08a3, 0x44);
	max96724_write(priv, 0x08a4, 0x44);
	max96724_write(priv, 0x08a5, 0x00);
	max96724_write(priv, 0x08a6, 0x00);
	max96724_write(priv, MAX96724_MIPI_CTRL_SEL, 0xe4);

	for (phy = 0; phy < 4; phy++) {
		base = 0x0900 + phy * 0x40;
		/* 2 data lanes */
		max96724_update_bits(priv, base + 0x0a, 0xc0, 0x40);
		max96724_update_bits(priv, MAX96724_DPLL_0(phy), 0x01, 0x00);
		max96724_update_bits(priv, 0x0415 + phy * 3, 0x3f, dpll);
		max96724_update_bits(priv, MAX96724_DPLL_0(phy), 0x01, 0x01);
		for (i = 0; i < ARRAY_SIZE(dts); i++) {
			max96724_write(priv, base + 0x0d + i * 2, dts[i]);
			max96724_write(priv, base + 0x0e + i * 2, dts[i]);
		}
		max96724_write(priv, base + 0x2d, 0x15 * phy);
		max96724_write(priv, base + 0x0b, 0x07);
		max96724_write(priv, base + 0x0c, 0x00);
	}

	max96724_update_bits(priv, 0x08a2, 0xf0, 0xf0);
}

static void max96724_mipi_configure(struct max96724_tpg *priv)
{
	static const u8 dts[] = { MIPI_DT_RGB888, MIPI_DT_FS, MIPI_DT_FE };
	const u8 dpll = ((MAX96724_DPLL_MBPS / 100) & 0x1f) | BIT(5);
	/* Other pipes also carry the pattern: park them on the unused port. */
	const u8 ctrl_sel = csi_port ? 0x02 : 0xa9;
	/* In 2x4 mode PHY1 is the master of port A and PHY2 of port B. */
	const unsigned int master = csi_port ? 2 : 1;
	const unsigned int first = csi_port ? 2 : 0;
	unsigned int i;

	max96724_mipi_enable(priv, false);

	if (quad_2lane) {
		max96724_mipi_configure_4x2(priv);
		return;
	}

	/* All PHYs to standby while the port is reconfigured. */
	max96724_update_bits(priv, 0x08a2, 0xf0, 0x00);

	/* 2x4 mode, 4-lane D-PHY on the selected port. */
	max96724_write(priv, 0x08a0, 0x04);
	max96724_update_bits(priv, 0x090a + master * 0x40, 0xc0, 0xc0);

	/* Lane mapping and polarity of the port. */
	max96724_write(priv, 0x08a3 + csi_port, lane_map);
	max96724_write(priv, 0x08a5 + csi_port, lane_polarity);

	/* Link frequency, with the master PHY DPLL held in reset meanwhile. */
	max96724_update_bits(priv, MAX96724_DPLL_0(master), 0x01, 0x00);
	max96724_update_bits(priv, 0x0415 + first * 3, 0x3f, dpll);
	max96724_update_bits(priv, 0x0418 + first * 3, 0x3f, dpll);
	max96724_update_bits(priv, MAX96724_DPLL_0(master), 0x01, 0x01);

	/* Route pipe 0 (pattern generator) to the master PHY: VC0 RGB888, FS, FE. */
	max96724_write(priv, MAX96724_MIPI_CTRL_SEL, ctrl_sel);
	for (i = 0; i < ARRAY_SIZE(dts); i++) {
		max96724_write(priv, MAX96724_MIPI_TX13_P0(i), dts[i]);
		max96724_write(priv, MAX96724_MIPI_TX14_P0(i), dts[i]);
	}
	max96724_write(priv, MAX96724_MIPI_TX45_P0, 0x15 * master);
	max96724_write(priv, MAX96724_MIPI_TX11_P0, 0x07);
	max96724_write(priv, MAX96724_MIPI_TX12_P0, 0x00);

	/* Take the port's two PHYs out of standby. */
	max96724_update_bits(priv, 0x08a2, 0xf0, csi_port ? 0xc0 : 0x30);
}

static void max96724_pattern_enable(struct max96724_tpg *priv, bool enable)
{
	const u32 h_active = 1920, h_fp = 88, h_sw = 44, h_bp = 148;
	const u32 h_tot = h_active + h_fp + h_sw + h_bp;
	const u32 v_active = 1080, v_fp = 4, v_sw = 16, v_bp = 36;
	const u32 v_tot = v_active + v_fp + v_sw + v_bp;

	if (!enable) {
		max96724_write(priv, 0x1051, 0x00);
		return;
	}

	/* 75 MHz pattern clock: 2200 x 1136 at 30 fps. */
	max96724_write(priv, MAX96724_DEBUG_EXTRA,
		       MAX96724_DEBUG_EXTRA_PCLK_75MHZ);

	/* Video Timing Generator, 1920x1080. */
	max96724_write_be(priv, 0x1052, 0, 3);
	max96724_write_be(priv, 0x1055, v_sw * h_tot, 3);
	max96724_write_be(priv, 0x1058, (v_active + v_fp + v_bp) * h_tot, 3);
	max96724_write_be(priv, 0x105b, 0, 3);
	max96724_write_be(priv, 0x105e, h_sw, 2);
	max96724_write_be(priv, 0x1060, h_active + h_fp + h_bp, 2);
	max96724_write_be(priv, 0x1062, v_tot, 2);
	max96724_write_be(priv, 0x1064, h_tot * (v_sw + v_bp) + (h_sw + h_bp), 3);
	max96724_write_be(priv, 0x1067, h_active, 2);
	max96724_write_be(priv, 0x1069, h_fp + h_sw + h_bp, 2);
	max96724_write_be(priv, 0x106b, v_active, 2);

	/* Generate VS (inverted), HS and DE in free-running mode. */
	max96724_write(priv, 0x1050, 0xf3);

	if (pattern == 0) {
		/* Checkerboard: square size, then the two colours. */
		max96724_write(priv, 0x1074, 0x3c);
		max96724_write(priv, 0x1075, 0x3c);
		max96724_write(priv, 0x1076, 0x3c);
		max96724_write_be(priv, 0x106e, 0x00ccfe, 3);
		max96724_write_be(priv, 0x1071, 0xa76a00, 3);
		max96724_write(priv, 0x1051, 0x10);
	} else {
		/* Gradient increment. */
		max96724_write(priv, 0x106d, 0x10);
		max96724_write(priv, 0x1051, 0x20);
	}
}

static int max96724_tpg_read_reg(struct camera_common_data *s_data,
				 u16 addr, u8 *val)
{
	u32 reg_val = 0;
	int err = regmap_read(s_data->regmap, addr, &reg_val);

	*val = reg_val & 0xff;

	return err;
}

static int max96724_tpg_write_reg(struct camera_common_data *s_data,
				  u16 addr, u8 val)
{
	return regmap_write(s_data->regmap, addr, val);
}

/* The pattern has no gain, exposure or frame rate to set. */
static int max96724_tpg_set_s64(struct tegracam_device *tc_dev, s64 val)
{
	return 0;
}

static int max96724_tpg_set_group_hold(struct tegracam_device *tc_dev,
				       bool val)
{
	return 0;
}

static struct tegracam_ctrl_ops max96724_tpg_ctrl_ops = {
	.numctrls = ARRAY_SIZE(ctrl_cid_list),
	.ctrl_cid_list = ctrl_cid_list,
	.set_gain = max96724_tpg_set_s64,
	.set_exposure = max96724_tpg_set_s64,
	.set_frame_rate = max96724_tpg_set_s64,
	.set_group_hold = max96724_tpg_set_group_hold,
};

/* PoC and the deserializer supply are managed by the carrier, not here. */
static int max96724_tpg_power_on(struct camera_common_data *s_data)
{
	s_data->power->state = SWITCH_ON;

	return 0;
}

static int max96724_tpg_power_off(struct camera_common_data *s_data)
{
	s_data->power->state = SWITCH_OFF;

	return 0;
}

static int max96724_tpg_power_get(struct tegracam_device *tc_dev)
{
	tc_dev->s_data->power->state = SWITCH_OFF;

	return 0;
}

static int max96724_tpg_power_put(struct tegracam_device *tc_dev)
{
	return 0;
}

static struct camera_common_pdata *
max96724_tpg_parse_dt(struct tegracam_device *tc_dev)
{
	return devm_kzalloc(tc_dev->dev, sizeof(struct camera_common_pdata),
			    GFP_KERNEL);
}

static int max96724_tpg_set_mode(struct tegracam_device *tc_dev)
{
	struct max96724_tpg *priv = tegracam_get_privdata(tc_dev);

	max96724_mipi_configure(priv);

	return 0;
}

static int max96724_tpg_start_streaming(struct tegracam_device *tc_dev)
{
	struct max96724_tpg *priv = tegracam_get_privdata(tc_dev);

	max96724_pattern_enable(priv, true);
	max96724_mipi_enable(priv, true);

	return 0;
}

static int max96724_tpg_stop_streaming(struct tegracam_device *tc_dev)
{
	struct max96724_tpg *priv = tegracam_get_privdata(tc_dev);

	max96724_mipi_enable(priv, false);
	max96724_pattern_enable(priv, false);

	return 0;
}

static struct camera_common_sensor_ops max96724_tpg_common_ops = {
	.numfrmfmts = ARRAY_SIZE(max96724_tpg_frmfmt),
	.frmfmt_table = max96724_tpg_frmfmt,
	.power_on = max96724_tpg_power_on,
	.power_off = max96724_tpg_power_off,
	.write_reg = max96724_tpg_write_reg,
	.read_reg = max96724_tpg_read_reg,
	.parse_dt = max96724_tpg_parse_dt,
	.power_get = max96724_tpg_power_get,
	.power_put = max96724_tpg_power_put,
	.set_mode = max96724_tpg_set_mode,
	.start_streaming = max96724_tpg_start_streaming,
	.stop_streaming = max96724_tpg_stop_streaming,
};

static int max96724_tpg_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	return 0;
}

static const struct v4l2_subdev_internal_ops max96724_tpg_internal_ops = {
	.open = max96724_tpg_open,
};

static int max96724_tpg_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct tegracam_device *tc_dev;
	struct max96724_tpg *priv;
	unsigned int id;
	int err;

	if (!client->dev.of_node)
		return -EINVAL;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	tc_dev = devm_kzalloc(dev, sizeof(*tc_dev), GFP_KERNEL);
	if (!priv || !tc_dev)
		return -ENOMEM;

	priv->i2c_client = tc_dev->client = client;
	tc_dev->dev = dev;
	strscpy(tc_dev->name, "max96724_tpg", sizeof(tc_dev->name));
	tc_dev->dev_regmap_config = &max96724_tpg_regmap_config;
	tc_dev->sensor_ops = &max96724_tpg_common_ops;
	tc_dev->v4l2sd_internal_ops = &max96724_tpg_internal_ops;
	tc_dev->tcctrl_ops = &max96724_tpg_ctrl_ops;

	err = tegracam_device_register(tc_dev);
	if (err) {
		dev_err(dev, "tegra camera driver registration failed\n");
		return err;
	}
	priv->tc_dev = tc_dev;
	priv->regmap = tc_dev->s_data->regmap;
	tegracam_set_privdata(tc_dev, priv);

	err = regmap_read(priv->regmap, MAX96724_REG_DEV_ID, &id);
	if (err || (id != MAX96724_DEV_ID && id != MAX96712_DEV_ID)) {
		dev_err(dev, "no MAX96724 found (err %d, id 0x%02x)\n", err, id);
		tegracam_device_unregister(tc_dev);
		return err ? err : -ENODEV;
	}

	err = tegracam_v4l2subdev_register(tc_dev, true);
	if (err) {
		tegracam_device_unregister(tc_dev);
		dev_err(dev, "tegra camera subdev registration failed\n");
		return err;
	}

	dev_info(dev, "MAX96724 (id 0x%02x) test pattern source ready\n", id);

	return 0;
}

static void max96724_tpg_remove(struct i2c_client *client)
{
	struct camera_common_data *s_data = to_camera_common_data(&client->dev);
	struct max96724_tpg *priv;

	if (!s_data)
		return;
	priv = s_data->priv;

	tegracam_v4l2subdev_unregister(priv->tc_dev);
	tegracam_device_unregister(priv->tc_dev);
}

static const struct i2c_device_id max96724_tpg_id[] = {
	{ "max96724_tpg", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, max96724_tpg_id);

static struct i2c_driver max96724_tpg_i2c_driver = {
	.driver = {
		.name = "max96724_tpg",
		.of_match_table = max96724_tpg_of_match,
	},
	.probe = max96724_tpg_probe,
	.remove = max96724_tpg_remove,
	.id_table = max96724_tpg_id,
};
module_i2c_driver(max96724_tpg_i2c_driver);

MODULE_DESCRIPTION("MAX96724 test pattern source for the PCB Arts Helium");
MODULE_LICENSE("GPL v2");
