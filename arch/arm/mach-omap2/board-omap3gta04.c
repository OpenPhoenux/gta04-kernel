/*
 * linux/arch/arm/mach-omap2/board-omap3gta04.c
 *
 * Copyright (C) 2008 Texas Instruments
 *
 * Modified from mach-omap2/board-omap3gta04.c
 *
 * Initial code: Syed Mohammed Khasim
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/leds.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/input.h>
#include <linux/gpio_keys.h>
#include <linux/backlight.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/nand.h>

#include <linux/regulator/machine.h>
#include <linux/i2c/twl.h>
#include <linux/i2c/tsc2007.h>

#include <linux/i2c/bmp085.h>

#include <linux/input/tca8418_keypad.h>

#include <../sound/soc/codecs/gtm601.h>
#include <../sound/soc/codecs/si47xx.h>
#include <../sound/soc/codecs/w2cbw003-bt.h>

#if defined(CONFIG_VIDEO_OV9655) || defined(CONFIG_VIDEO_OV9655_MODULE)
#include <media/v4l2-int-device.h>
#include <media/ov9655.h>
extern struct ov9655_platform_data ov9655_pdata;
#endif

#include <linux/mmc/host.h>

#include <linux/sysfs.h>

#include <mach/hardware.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/flash.h>

#include <plat/board.h>
#include <plat/common.h>
#include <plat/display.h>
#include <plat/gpmc.h>
#include <plat/nand.h>
#include <plat/usb.h>
#include <plat/timer-gp.h>
#include <plat/clock.h>
#include <plat/omap-pm.h>
#include <plat/mcspi.h>
#include <plat/control.h>

#include "mux.h"
#include "mmc-twl4030.h"
#include "pm.h"
#include "omap3-opp.h"

#ifdef CONFIG_PM
static struct omap_opp * _omap35x_mpu_rate_table        = omap35x_mpu_rate_table;
static struct omap_opp * _omap37x_mpu_rate_table        = omap37x_mpu_rate_table;
static struct omap_opp * _omap35x_dsp_rate_table        = omap35x_dsp_rate_table;
static struct omap_opp * _omap37x_dsp_rate_table        = omap37x_dsp_rate_table;
static struct omap_opp * _omap35x_l3_rate_table         = omap35x_l3_rate_table;
static struct omap_opp * _omap37x_l3_rate_table         = omap37x_l3_rate_table;
#else   /* CONFIG_PM */
static struct omap_opp * _omap35x_mpu_rate_table        = NULL;
static struct omap_opp * _omap37x_mpu_rate_table        = NULL;
static struct omap_opp * _omap35x_dsp_rate_table        = NULL;
static struct omap_opp * _omap37x_dsp_rate_table        = NULL;
static struct omap_opp * _omap35x_l3_rate_table         = NULL;
static struct omap_opp * _omap37x_l3_rate_table         = NULL;
#endif  /* CONFIG_PM */


#define GPMC_CS0_BASE  0x60
#define GPMC_CS_SIZE   0x30

#define NAND_BLOCK_SIZE		SZ_128K

/* definitions of some GPIOs */

#define AUX_BUTTON_GPIO	7
#define TWL4030_MSECURE_GPIO 22
#define TS_PENIRQ_GPIO		160

/*
 This interrupt line is pulsed by the UMTS modem
 on an incoming call or SMS and is intended to wake
 up the CPU.
 For testing purposes we just assume the CPU isn't
 suspended at all and can catch the interrupt.
 For a fully optimized system the CPU should go
 to sleep as often as possible and the power management
 should be configured that either this WO3G_GPIO,
 or a TS_PENIRQ_GPIO or the AUX, Power Button or USB
 event can wake up.
 Maybe, the type of the wakup event should be notified
 to user space as a button-press.
 */

#define WO3G_GPIO	(gta04_version >= 4 ? 10 : 176)	/* changed on A4 boards */
#define KEYIRQ_GPIO	(gta04_version >= 4 ? 176 : 10)	/* changed on A4 boards */

/* compare with: https://patchwork.kernel.org/patch/120449/
 * OMAP3 gta04 revision
 * Run time detection of gta04 revision is done by reading GPIO.
 */

static u8 gta04_version;	/* counts 2..9 */

static void __init gta04_init_rev(void)
{
	int ret;
	u16 gta04_rev = 0;
	static char revision[8] = {	/* revision table defined by pull-down R305, R306, R307 */
		9,
		6,
		7,
		3,
		8,
		4,
		5,
		2
	};
	
#ifndef CONFIG_OMAP_MUX
#error we need CONFIG_OMAP_MUX
#endif

	omap_mux_init_gpio(171, OMAP_PIN_INPUT_PULLUP);
	omap_mux_init_gpio(172, OMAP_PIN_INPUT_PULLUP);
	omap_mux_init_gpio(173, OMAP_PIN_INPUT_PULLUP);

	udelay(100);

	ret = gpio_request(171, "rev_id_0");
	if (ret < 0)
		goto fail0;
	
	ret = gpio_request(172, "rev_id_1");
	if (ret < 0)
		goto fail1;
	
	ret = gpio_request(173, "rev_id_2");
	if (ret < 0)
		goto fail2;

	udelay(100);

	gpio_direction_input(171);
	gpio_direction_input(172);
	gpio_direction_input(173);
	
	udelay(100);

	gta04_rev = gpio_get_value(171)
				| (gpio_get_value(172) << 1)
				| (gpio_get_value(173) << 2);
	
	printk("gta04_rev %u\n", gta04_rev);

	gta04_version = revision[gta04_rev];
	
	return;
	
fail2:
	gpio_free(172);
fail1:
	gpio_free(171);
fail0:
	printk(KERN_ERR "Unable to get revision detection GPIO pins\n");
	gta04_version = 0;

	return;
}


static struct mtd_partition gta04_nand_partitions[] = {
	/* All the partition sizes are listed in terms of NAND block size */
	{
		.name		= "X-Loader",
		.offset		= 0,
		.size		= 4 * NAND_BLOCK_SIZE,
		.mask_flags	= MTD_WRITEABLE,	/* force read-only */
	},
	{
		.name		= "U-Boot",
		.offset		= MTDPART_OFS_APPEND,	/* Offset = 0x80000 */
		.size		= 15 * NAND_BLOCK_SIZE,
	},
	{
		.name		= "U-Boot Env",
		.offset		= MTDPART_OFS_APPEND,	/* Offset = 0x260000 */
		.size		= 1 * NAND_BLOCK_SIZE,
	},
	{
		.name		= "Kernel",
		.offset		= MTDPART_OFS_APPEND,	/* Offset = 0x280000 */
		.size		= 32 * NAND_BLOCK_SIZE,
	},
	{
		.name		= "File System",
		.offset		= MTDPART_OFS_APPEND,	/* Offset = 0x680000 */
		.size		= MTDPART_SIZ_FULL,
	},
};

static struct omap_nand_platform_data gta04_nand_data = {
	.options	= NAND_BUSWIDTH_16,
	.parts		= gta04_nand_partitions,
	.nr_parts	= ARRAY_SIZE(gta04_nand_partitions),
	.dma_channel	= -1,		/* disable DMA in OMAP NAND driver */
	.nand_setup	= NULL,
	.dev_ready	= NULL,
};

static struct resource gta04_nand_resource = {
	.flags		= IORESOURCE_MEM,
};

static struct platform_device gta04_nand_device = {
	.name		= "omap2-nand",
	.id		= -1,
	.dev		= {
		.platform_data	= &gta04_nand_data,
	},
	.num_resources	= 1,
	.resource	= &gta04_nand_resource,
};

/* DSS */

static int gta04_enable_dvi(struct omap_dss_device *dssdev)
{
	if (dssdev->reset_gpio != -1)
		gpio_set_value(dssdev->reset_gpio, 1);

	return 0;
}

static void gta04_disable_dvi(struct omap_dss_device *dssdev)
{
	if (dssdev->reset_gpio != -1)
		gpio_set_value(dssdev->reset_gpio, 0);
}

static struct omap_dss_device gta04_dvi_device = {
	.type = OMAP_DISPLAY_TYPE_DPI,
	.name = "dvi",
	.driver_name = "generic_panel",
	.phy.dpi.data_lines = 24,
//	.reset_gpio = 170,
	.reset_gpio = -1,
	.platform_enable = gta04_enable_dvi,
	.platform_disable = gta04_disable_dvi,
};

static int gta04_enable_lcd(struct omap_dss_device *dssdev)
{
	printk("gta04_enable_lcd()\n");
	// whatever we need, e.g. enable power
	gpio_set_value(57, 1);	// enable backlight
	return 0;
}

static void gta04_disable_lcd(struct omap_dss_device *dssdev)
{
	printk("gta04_disable_lcd()\n");
	// whatever we need, e.g. disable power
	gpio_set_value(57, 0);	// shut down backlight
}

static struct omap_dss_device gta04_lcd_device = {
	.type = OMAP_DISPLAY_TYPE_DPI,
	.name = "lcd",
#if defined(CONFIG_PANEL_ORTUS_COM37H3M05DTC)
	.driver_name = "com37h3m05dtc_panel",		// GTA04b2
#elif defined(CONFIG_PANEL_TPO_TD028TTEC1)
	.driver_name = "td028ttec1_panel",			// GTA04
#elif defined(CONFIG_PANEL_SHARP_LQ070Y3DG3B)
	.driver_name = "lq070y3dg3b_panel",			// GTA04b3
#elif defined(CONFIG_PANEL_SHARP_LQ050W1LC1B)
	.driver_name = "lq050w1lc1b_panel",			// GTA04b4
#endif
	.phy.dpi.data_lines = 24,
	.platform_enable = gta04_enable_lcd,
	.platform_disable = gta04_disable_lcd,
};

static int gta04_panel_enable_tv(struct omap_dss_device *dssdev)
{
	u32 reg;
	
#define ENABLE_VDAC_DEDICATED           0x03
#define ENABLE_VDAC_DEV_GRP             0x20
#define OMAP2_TVACEN				(1 << 11)
#define OMAP2_TVOUTBYPASS			(1 << 18)

	twl_i2c_write_u8(TWL4030_MODULE_PM_RECEIVER,
			ENABLE_VDAC_DEDICATED,
			TWL4030_VDAC_DEDICATED);
	twl_i2c_write_u8(TWL4030_MODULE_PM_RECEIVER,
			ENABLE_VDAC_DEV_GRP, TWL4030_VDAC_DEV_GRP);

	/* taken from https://e2e.ti.com/support/dsp/omap_applications_processors/f/447/p/94072/343691.aspx */
	reg = omap_ctrl_readl(OMAP343X_CONTROL_DEVCONF1);
//	printk(KERN_INFO "Value of DEVCONF1 was: %08x\n", reg);
	reg |= OMAP2_TVOUTBYPASS;	/* enable TV bypass mode for external video driver (OPA362) */
	reg |= OMAP2_TVACEN;		/* assume AC coupling to remove DC offset */
	omap_ctrl_writel(reg, OMAP343X_CONTROL_DEVCONF1);
	reg = omap_ctrl_readl(OMAP343X_CONTROL_DEVCONF1);
//	printk(KERN_INFO "Value of DEVCONF1 now: %08x\n", reg);  
	
	gpio_set_value(23, 1);	// enable output driver (OPA362)
	
	return 0;
}

static void gta04_panel_disable_tv(struct omap_dss_device *dssdev)
{
	gpio_set_value(23, 0);	// disable output driver (and re-enable microphone)
	
	twl_i2c_write_u8(TWL4030_MODULE_PM_RECEIVER, 0x00,
			TWL4030_VDAC_DEDICATED);
	twl_i2c_write_u8(TWL4030_MODULE_PM_RECEIVER, 0x00,
			TWL4030_VDAC_DEV_GRP);
}

static struct omap_dss_device gta04_tv_device = {
	.name = "tv",
	.driver_name = "venc",
	.type = OMAP_DISPLAY_TYPE_VENC,
	/* GTA04 has a single composite output (with external video driver) */
	.phy.venc.type = OMAP_DSS_VENC_TYPE_COMPOSITE, /*OMAP_DSS_VENC_TYPE_SVIDEO, */
	.phy.venc.invert_polarity = true,	/* needed if we use external video driver */
	.platform_enable = gta04_panel_enable_tv,
	.platform_disable = gta04_panel_disable_tv,
};

static struct omap_dss_device *gta04_dss_devices[] = {
	&gta04_dvi_device,
	&gta04_tv_device,
	&gta04_lcd_device,
};

static struct omap_dss_board_info gta04_dss_data = {
	.num_devices = ARRAY_SIZE(gta04_dss_devices),
	.devices = gta04_dss_devices,
	.default_device = &gta04_dvi_device,
};

static struct platform_device gta04_dss_device = {
	.name          = "omapdss",
	.id            = -1,
	.dev            = {
		.platform_data = &gta04_dss_data,
	},
};

static void gta04_set_bl_intensity(int intensity)
{
	// control PWM_11
	// use 500 Hz pulse and intensity 0..255
}

static struct generic_bl_info gta04_bl_platform_data = {
	.name			= "bklight",
	.max_intensity		= 255,
	.default_intensity	= 200,
	.limit_mask		= 0,
	.set_bl_intensity	= gta04_set_bl_intensity,
	.kick_battery		= NULL,
};

static struct platform_device gta04_bklight_device = {
	.name		= "generic-bl",
	.id			= -1,
	.dev		= {
		.parent		= &gta04_dss_device.dev,
		.platform_data	= &gta04_bl_platform_data,
	},
};


static struct regulator_consumer_supply gta04_vdac_supply = {
	.supply		= "vdda_dac",
	.dev		= &gta04_dss_device.dev,
};

static struct regulator_consumer_supply gta04_vdvi_supply = {
	.supply		= "vdds_dsi",
	.dev		= &gta04_dss_device.dev,
};

static void __init gta04_display_init(void)
{
/* N/A on GTA04
	int r;
 
	r = gpio_request(gta04_dvi_device.reset_gpio, "DVI reset");
	if (r < 0) {
		printk(KERN_ERR "Unable to get DVI reset GPIO %d\n", gta04_dvi_device.reset_gpio);
		return;
	}

	gpio_direction_output(gta04_dvi_device.reset_gpio, 0);
 */
}

#include "sdram-micron-mt46h32m32lf-6.h"

static struct gpio_led gpio_leds[];

static struct twl4030_hsmmc_info mmc[] = {
	{
		.mmc		= 1,
//		.wires		= 8,
		.wires		= 4,	// only 4 are connected
		.gpio_cd	= -EINVAL,	// no card detect
//		.gpio_wp	= 29,
		.gpio_wp	= -EINVAL,	// no write protect
	},
	{ // this is the WiFi SDIO interface
		.mmc		= 2,
		.wires		= 4,
		.gpio_cd	= -EINVAL,	// no card detect
		.gpio_wp	= -EINVAL,	// no write protect
		.transceiver	= true,	// external transceiver
		.ocr_mask	= MMC_VDD_27_28 | MMC_VDD_28_29 | MMC_VDD_29_30
					| MMC_VDD_30_31 | MMC_VDD_31_32 | MMC_VDD_32_33,	/* VDD voltage 2.7 ~ 3.3 */
	},
	{}	/* Terminator */
};

static struct regulator_consumer_supply gta04_vmmc1_supply = {
	.supply			= "vmmc",
};

static struct regulator_consumer_supply gta04_vsim_supply = {
	.supply			= "vmmc_aux",	// FIXME: we use VSIM to power the GPS antenna with 2.8V
};

static int gta04_twl_gpio_setup(struct device *dev,
		unsigned gpio, unsigned ngpio)
{
	// we should keep enabling mmc, vmmc1, nEN_USB_PWR, maybe CAM_EN
	
	twl4030_mmc_init(mmc);

	/* link regulators to MMC adapters */
	gta04_vmmc1_supply.dev = mmc[0].dev;

	return 0;
}

static struct platform_device gta04_cam_device = {
	.name		= "gta04_cam",
	.id		= -1,
};

static struct twl4030_gpio_platform_data gta04_gpio_data = {
	.gpio_base	= OMAP_MAX_GPIO_LINES,
	.irq_base	= TWL4030_GPIO_IRQ_BASE,
	.irq_end	= TWL4030_GPIO_IRQ_END,
	.use_leds	= true,
	.pullups	= BIT(1),
	.pulldowns	= BIT(2) | BIT(6) | BIT(7) | BIT(8) | BIT(13)
				| BIT(15) | BIT(16) | BIT(17),
	.setup		= gta04_twl_gpio_setup,
};

/* VMMC1 for MMC1 pins CMD, CLK, DAT0..DAT3 (20 mA, plus card == max 220 mA) */
static struct regulator_init_data gta04_vmmc1 = {
	.constraints = {
		.name			= "VMMC1",
		.min_uV			= 1850000,
		.max_uV			= 3150000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &gta04_vmmc1_supply,
};

/* VAUX4 powers Bluetooth and WLAN */

static struct regulator_consumer_supply gta04_vaux4_supply = {
	.supply			= "vaux4",
};

static struct regulator_init_data gta04_vaux4 = {
	.constraints = {
		.name			= "VAUX4",
		.min_uV			= 2800000,
#ifdef CONFIG_TWL4030_ALLOW_UNSUPPORTED
		.max_uV			= 3150000,	// FIXME: this is a HW issue - 3.15V or 3.3V isn't supported officially - set CONFIG_TWL4030_ALLOW_UNSUPPORTED
#else
		.max_uV			= 2800000,
#endif
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
		| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_VOLTAGE
		| REGULATOR_CHANGE_MODE
		| REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &gta04_vaux4_supply,
};

/* VAUX3 for Camera */

static struct regulator_consumer_supply gta04_vaux3_supply = {
	.supply			= "cam_2v5",
	.dev		= &gta04_cam_device.dev,
};

static struct regulator_init_data gta04_vaux3 = {
	.constraints = {
		.name			= "VAUX3",
		.min_uV			= 2500000,
		.max_uV			= 2500000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
		| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= /* REGULATOR_CHANGE_VOLTAGE
		| REGULATOR_CHANGE_MODE
		| */ REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &gta04_vaux3_supply,
};

/* VAUX2 for Sensors ITG3200 (and LIS302/LSM303) */

static struct regulator_consumer_supply gta04_vaux2_supply = {
	.supply			= "vaux2",
};

static struct regulator_init_data gta04_vaux2 = {
	.constraints = {
		.name			= "VAUX2",
		.min_uV			= 2800000,
		.max_uV			= 2800000,
		.always_on		= 1,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
	/*	| REGULATOR_MODE_STANDBY */,
		.valid_ops_mask		= 0 /* REGULATOR_CHANGE_VOLTAGE
		| REGULATOR_CHANGE_MODE
		| REGULATOR_CHANGE_STATUS */,
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &gta04_vaux2_supply,
};

/* VAUX1 unused */

static struct regulator_consumer_supply gta04_vaux1_supply = {
	.supply			= "vaux1",
};

static struct regulator_init_data gta04_vaux1 = {
	.constraints = {
		.name			= "VAUX1",
		.min_uV			= 2500000,
		.max_uV			= 3000000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
		| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_VOLTAGE
		| REGULATOR_CHANGE_MODE
		| REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &gta04_vaux1_supply,
};

/* VSIM used for powering the external GPS Antenna */

static struct regulator_init_data gta04_vsim = {
	.constraints = {
		.name			= "VSIM",
		.min_uV			= 2800000,
		.max_uV			= 3000000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &gta04_vsim_supply,	// vmmc_aux
};

/* VDAC for DSS driving S-Video (8 mA unloaded, max 65 mA) */
static struct regulator_init_data gta04_vdac = {
	.constraints = {
		.name			= "VDAC",
		.min_uV			= 1800000,
		.max_uV			= 1800000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &gta04_vdac_supply,
};

/* VPLL2 for digital video outputs */
static struct regulator_init_data gta04_vpll2 = {
	.constraints = {
		.name			= "VDVI",
		.min_uV			= 1800000,
		.max_uV			= 1800000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &gta04_vdvi_supply,
};

static struct twl4030_usb_data gta04_usb_data = {
	.usb_mode	= T2_USB_MODE_ULPI,
};

static struct twl4030_codec_audio_data gta04_audio_data = {
	.audio_mclk = 26000000,
};

static struct twl4030_codec_data gta04_codec_data = {
	.audio_mclk = 26000000,
	.audio = &gta04_audio_data,
};

static struct twl4030_madc_platform_data gta04_madc_data = {
	.irq_line	= 1,
};

// FIXME: we could copy more scripts from board-sdp3430.c if we understand what they do... */

struct twl4030_power_data gta04_power_scripts = {
/*	.scripts	= NULL,	*/
	.num		= 0,
/*	.resource_config	= NULL; */
};

/* override TWL defaults */

static int gta04_batt_table[] = {
	/* 0 C*/
	30800, 29500, 28300, 27100,
	26000, 24900, 23900, 22900, 22000, 21100, 20300, 19400, 18700, 17900,
	17200, 16500, 15900, 15300, 14700, 14100, 13600, 13100, 12600, 12100,
	11600, 11200, 10800, 10400, 10000, 9630,  9280,  8950,  8620,  8310,
	8020,  7730,  7460,  7200,  6950,  6710,  6470,  6250,  6040,  5830,
	5640,  5450,  5260,  5090,  4920,  4760,  4600,  4450,  4310,  4170,
	4040,  3910,  3790,  3670,  3550
};

static struct twl4030_bci_platform_data gta04_bci_data = {
	.battery_tmp_tbl        = gta04_batt_table,
	.tblsize                = ARRAY_SIZE(gta04_batt_table),
};


static struct twl4030_platform_data gta04_twldata = {
	.irq_base	= TWL4030_IRQ_BASE,
	.irq_end	= TWL4030_IRQ_END,

	/* platform_data for children goes here */
	.bci		= &gta04_bci_data,
	.gpio		= &gta04_gpio_data,
	.madc		= &gta04_madc_data,
	.power		= &gta04_power_scripts,	/* empty but if not present, pm_power_off is not initialized */
	.usb		= &gta04_usb_data,
	.codec		= &gta04_codec_data,

	.vaux1		= &gta04_vaux1,
	.vaux2		= &gta04_vaux2,
	.vaux3		= &gta04_vaux3,
	.vaux4		= &gta04_vaux4,
	.vmmc1		= &gta04_vmmc1,
	.vsim		= &gta04_vsim,
	.vdac		= &gta04_vdac,
	.vpll2		= &gta04_vpll2,
};

static struct i2c_board_info __initdata gta04_i2c1_boardinfo[] = {
	{
		I2C_BOARD_INFO("twl4030", 0x48),
		.flags = I2C_CLIENT_WAKE,
		.irq = INT_34XX_SYS_NIRQ,
		.platform_data = &gta04_twldata,
	},
};

	
#if defined(CONFIG_SND_SOC_GTM601)

static struct platform_device gta04_gtm601_codec_audio_device = {
	.name	= "gtm601_codec_audio",
	.id	= -1,
	.dev	= {
		.platform_data	= NULL,
	},
};
#endif

#if defined(CONFIG_SND_SOC_SI47XX)

static struct platform_device gta04_si47xx_codec_audio_device = {
	.name	= "si47xx_codec_audio",
	.id	= -1,
	.dev	= {
		.platform_data	= NULL,
	},
};
#endif

#if defined(CONFIG_SND_SOC_W2CBW003)

static struct platform_device gta04_w2cbw003_codec_audio_device = {
	.name	= "w2cbw003_codec_audio",
	.id	= -1,
	.dev	= {
		.platform_data	= NULL,
	},
};
#endif

#if defined(CONFIG_TOUCHSCREEN_TSC2007) || defined(CONFIG_TOUCHSCREEN_TSC2007_MODULE)

// TODO: see also http://e2e.ti.com/support/arm174_microprocessors/omap_applications_processors/f/42/t/33262.aspx for an example...
// and http://www.embedded-bits.co.uk/?tag=struct-i2c_board_info for a description of how struct i2c_board_info works

/* TouchScreen */

static int ts_get_pendown_state(void)
{
	int val;

	val = gpio_get_value(TS_PENIRQ_GPIO);
//	printk("ts_get_pendown_state() -> %d\n", val);
	return val ? 0 : 1;
}

static int __init tsc2007_init(void)
{
	printk("tsc2007_init()\n");
	omap_mux_init_gpio(TS_PENIRQ_GPIO, OMAP_PIN_INPUT_PULLUP);
	if (gpio_request(TS_PENIRQ_GPIO, "tsc2007_pen_down")) {
		printk(KERN_ERR "Failed to request GPIO %d for "
			   "TSC2007 pen down IRQ\n", TS_PENIRQ_GPIO);
		return  -ENODEV;
	}
	
	if (gpio_direction_input(TS_PENIRQ_GPIO)) {
		printk(KERN_WARNING "GPIO#%d cannot be configured as "
			   "input\n", TS_PENIRQ_GPIO);
		return -ENXIO;
	}
	gpio_export(TS_PENIRQ_GPIO, 0);
	omap_set_gpio_debounce(TS_PENIRQ_GPIO, 1);
	omap_set_gpio_debounce_time(TS_PENIRQ_GPIO, 0xa);	// means (10+1)x31 us
	set_irq_type(OMAP_GPIO_IRQ(TS_PENIRQ_GPIO), IRQ_TYPE_EDGE_FALLING);
	return 0;
}

static void tsc2007_exit(void)
{
	gpio_free(TS_PENIRQ_GPIO);
}

struct tsc2007_platform_data __initdata tsc2007_info = {
	.model			= 2007,
#if defined(CONFIG_PANEL_ORTUS_COM37H3M05DTC)
	.x_plate_ohms		= 600,		// GTA04b2: 200 - 900
#elif defined(CONFIG_PANEL_TPO_TD028TTEC1)
	.x_plate_ohms		= 550,			// GTA04: 250 - 900
#elif defined(CONFIG_PANEL_SHARP_LQ070Y3DG3B)
	.x_plate_ohms		= 450,			// GTA04b3: 100 - 900
#elif defined(CONFIG_PANEL_SHARP_LQ050W1LC1B)
	.x_plate_ohms		= 400,			// GTA04b4: 100 - 850 (very asymmetric between X and Y!)
#endif
	.get_pendown_state	= ts_get_pendown_state,
	.init_platform_hw	= tsc2007_init,
	.exit_platform_hw	= tsc2007_exit,
};

#endif


#if defined(CONFIG_BMP085) || defined(CONFIG_BMP085_MODULE)

#define BMP085_EOC_IRQ_GPIO		113	/* BMP085 end of conversion GPIO */

static int __init bmp085_init(void)
{
	printk("bmp085_init()\n");
	omap_mux_init_gpio(BMP085_EOC_IRQ_GPIO, OMAP_PIN_INPUT_PULLUP);
	if (gpio_request(BMP085_EOC_IRQ_GPIO, "bmp085_eoc_irq")) {
		printk(KERN_ERR "Failed to request GPIO %d for "
			   "BMP085 EOC IRQ\n", BMP085_EOC_IRQ_GPIO);
		return  -ENODEV;
	}
	
	if (gpio_direction_input(BMP085_EOC_IRQ_GPIO)) {
		printk(KERN_WARNING "GPIO#%d cannot be configured as "
			   "input\n", BMP085_EOC_IRQ_GPIO);
		return -ENXIO;
	}
//	gpio_export(BMP085_EOC_IRQ_GPIO, 0);
	omap_set_gpio_debounce(BMP085_EOC_IRQ_GPIO, 1);
	omap_set_gpio_debounce_time(BMP085_EOC_IRQ_GPIO, 0xa);
	set_irq_type(OMAP_GPIO_IRQ(BMP085_EOC_IRQ_GPIO), IRQ_TYPE_EDGE_FALLING);
	return 0;
}

static void bmp085_exit(void)
{
	gpio_free(BMP085_EOC_IRQ_GPIO);
}

struct bmp085_platform_data __initdata bmp085_info = {
	.init_platform_hw	= bmp085_init,
	.exit_platform_hw	= bmp085_exit,
};

#endif

#if defined(CONFIG_KEYBOARD_TCA8418) || defined(CONFIG_KEYBOARD_TCA8418_MODULE)

const uint32_t gta04_keymap[] = {
	/* KEY(row, col, val) - see include/linux/input.h */
	KEY(0, 0, KEY_LEFTCTRL),
	KEY(0, 1, KEY_RIGHTCTRL),
	KEY(0, 2, KEY_Y),
	KEY(0, 3, KEY_A),
	KEY(0, 4, KEY_Q),
	KEY(0, 5, KEY_1),
//	KEY(0, 6, KEY_RESERVED),
//	KEY(0, 7, KEY_RESERVED),
	KEY(0, 8, KEY_SPACE),
	KEY(0, 9, KEY_OK),

	KEY(1, 0, KEY_LEFTALT),
	KEY(1, 1, KEY_FN),
	KEY(1, 2, KEY_SPACE),
	KEY(1, 3, KEY_SPACE),
	KEY(1, 4, KEY_COMMA),
	KEY(1, 5, KEY_DOT),
	KEY(1, 6, KEY_PAGEDOWN),
	KEY(1, 7, KEY_END),
	KEY(1, 8, KEY_LEFT),
	KEY(1, 9, KEY_RIGHT),
	
	KEY(2, 0, KEY_DELETE),
//	KEY(2, 1, KEY_RESERVED),
	KEY(2, 2, KEY_TAB),
	KEY(2, 3, KEY_BACKSPACE),
	KEY(2, 4, KEY_ENTER),
	KEY(2, 5, KEY_GRAVE),
	KEY(2, 6, KEY_PAGEUP),
	KEY(2, 7, KEY_HOME),
	KEY(2, 8, KEY_UP),
	KEY(2, 9, KEY_DOWN),

	KEY(3, 0, KEY_RIGHTALT),
	KEY(3, 1, KEY_CAPSLOCK),
	KEY(3, 2, KEY_ESC),
//	KEY(3, 3, KEY_RESERVED),
//	KEY(3, 4, KEY_RESERVED),
//	KEY(3, 5, KEY_RESERVED),
	KEY(3, 6, KEY_LEFTBRACE),
	KEY(3, 7, KEY_RIGHTBRACE),
	KEY(3, 8, KEY_SEMICOLON),
	KEY(3, 9, KEY_APOSTROPHE),

	KEY(4, 0, KEY_LEFTSHIFT),
	KEY(4, 1, KEY_X),
	KEY(4, 2, KEY_C),
	KEY(4, 3, KEY_V),
	KEY(4, 4, KEY_B),
	KEY(4, 5, KEY_N),
	KEY(4, 6, KEY_M),
	KEY(4, 7, KEY_MINUS),
	KEY(4, 8, KEY_EQUAL),
	KEY(4, 9, KEY_KPASTERISK),

	KEY(5, 0, KEY_RIGHTSHIFT),
	KEY(5, 1, KEY_S),
	KEY(5, 2, KEY_D),
	KEY(5, 3, KEY_F),
	KEY(5, 4, KEY_G),
	KEY(5, 5, KEY_H),
	KEY(5, 6, KEY_J),
	KEY(5, 7, KEY_K),
	KEY(5, 8, KEY_L),
	KEY(5, 9, KEY_APOSTROPHE),
	
	KEY(6, 0, KEY_LEFTMETA),
	KEY(6, 1, KEY_W),
	KEY(6, 2, KEY_E),
	KEY(6, 3, KEY_R),
	KEY(6, 4, KEY_T),
	KEY(6, 5, KEY_Z),
	KEY(6, 6, KEY_U),
	KEY(6, 7, KEY_I),
	KEY(6, 8, KEY_O),
	KEY(6, 9, KEY_P),
	
	KEY(7, 0, KEY_RIGHTMETA),
	KEY(7, 1, KEY_2),
	KEY(7, 2, KEY_3),
	KEY(7, 3, KEY_4),
	KEY(7, 4, KEY_5),
	KEY(7, 5, KEY_6),
	KEY(7, 6, KEY_7),
	KEY(7, 7, KEY_8),
	KEY(7, 8, KEY_9),
	KEY(7, 9, KEY_0),
};

struct matrix_keymap_data tca8418_keymap = {
	.keymap = gta04_keymap,
	.keymap_size = ARRAY_SIZE(gta04_keymap),
};

struct tca8418_keypad_platform_data tca8418_pdata = {
	.keymap_data = &tca8418_keymap,
	.rows = 8,
	.cols = 10,
	.rep = 1,
	.irq_is_gpio = 1,
	};

#endif

static struct i2c_board_info __initdata gta04_i2c2_boardinfo[] = {
#if defined(CONFIG_LIS302) || defined(CONFIG_LIS302_MODULE)
	{
	I2C_BOARD_INFO("lis302top", 0x1c),
	.type		= "lis302",
	.platform_data	= &lis302_info,
	.irq		=  115,
	},
	{
	I2C_BOARD_INFO("lis302bottom", 0x1d),
	.type		= "lis302",
	.platform_data	= &lis302_info,
	.irq		=  114,
	},
#endif
#if defined(CONFIG_BMP085) || defined(CONFIG_BMP085_MODULE)
	{
	I2C_BOARD_INFO("bmp085", 0x77),
	.type		= "bmp085",
	.platform_data	= &bmp085_info,
	.irq		=  OMAP_GPIO_IRQ(BMP085_EOC_IRQ_GPIO),
	},
#endif
#if defined(CONFIG_ITG3200) || defined(CONFIG_ITG3200_MODULE)
	{
	I2C_BOARD_INFO("itg3200", 0x68),
	.type		= "itg3200",
	.platform_data	= NULL,
	.irq		= 56,
	},	
#endif
#if defined(CONFIG_TOUCHSCREEN_TSC2007) || defined(CONFIG_TOUCHSCREEN_TSC2007_MODULE)
{
	I2C_BOARD_INFO("tsc2007", 0x48),
	.type		= "tsc2007",
	.platform_data	= &tsc2007_info,
	.irq		=  OMAP_GPIO_IRQ(TS_PENIRQ_GPIO),
},
#endif
#if defined(CONFIG_BMA180) || defined(CONFIG_BMA180_MODULE)
	{
	I2C_BOARD_INFO("bma180", 0x41),
	.type		= "bma180",
	.platform_data	= NULL,
	.irq		= 115,
	},	
#endif
#if defined(CONFIG_BMA250) || defined(CONFIG_BMA250_MODULE)
	{
	I2C_BOARD_INFO("bma250", 0x18),
	.type		= "bma250",
	.platform_data	= NULL,
	.irq		= 115,
	},	
#endif
#if defined(CONFIG_BMC050) || defined(CONFIG_BMC050_MODULE)
	{
	I2C_BOARD_INFO("bmc050", 0x10),
	.type		= "bmc050",
	.platform_data	= NULL,
	.irq		= 111,
	},	
#endif
#if defined(CONFIG_HMC5883L) || defined(CONFIG_HMC5883L_MODULE)
	{
	I2C_BOARD_INFO("hmc5883l", 0x1e),
	.type		= "hmc5883l",
	.platform_data	= NULL,
	.irq		= 111,
	},	
#endif
#if defined(CONFIG_LEDS_TCA6507) || defined(CONFIG_LEDS_TCA6507_MODULE)
	{
	I2C_BOARD_INFO("tca6507", 0x45),
	.type		= "tca6507",
	.platform_data	= NULL,
	},	
#endif
#if defined(CONFIG_TPS61050) || defined(CONFIG_TPS61050_MODULE)
	{
	I2C_BOARD_INFO("tps61050", 0x33),
	.type		= "tps61050",
	.platform_data	= NULL,
	.irq		= -EINVAL,
	},	
#endif
#if defined(CONFIG_EEPROM_AT24) || defined(CONFIG_EEPROM_AT24_MODULE)
	{
	I2C_BOARD_INFO("24c64", 0x50),
	.type		= "mt24lr64",
	.platform_data	= NULL,
	.irq		= -EINVAL,
	},	
#endif
#if defined(CONFIG_KEYBOARD_TCA8418) || defined(CONFIG_KEYBOARD_TCA8418_MODULE)
	{
	I2C_BOARD_INFO("tca8418", 0x34),	/* /sys/.../name */
	.type		= "tca8418_keypad",	/* driver name */
	.platform_data	= &tca8418_pdata,
	.irq		= -EINVAL,	// will be modified dynamically by code
	},	
#endif
#if defined(CONFIG_VIDEO_OV9655) || defined(CONFIG_VIDEO_OV9655_MODULE)
	{
	I2C_BOARD_INFO("ov9655", OV9655_I2C_ADDR),
	.type		= "ov9655",
	.platform_data	= &ov9655_pdata,
	.irq		= -EINVAL,
	},
#endif
	};

static struct i2c_board_info __initdata gta04_i2c3_boardinfo[] = {
	/* Bus 3 is currently not used */
	/* add your I2C_BOARD_INFO records here */
};

static int __init gta04_i2c_init(void)
{
	omap_register_i2c_bus(1, 2600, gta04_i2c1_boardinfo,
			ARRAY_SIZE(gta04_i2c1_boardinfo));
	omap_register_i2c_bus(2, 400,  gta04_i2c2_boardinfo,
			ARRAY_SIZE(gta04_i2c2_boardinfo));
	omap_register_i2c_bus(3, 100, gta04_i2c3_boardinfo,
			ARRAY_SIZE(gta04_i2c3_boardinfo));
	return 0;
}

#if 0
// FIXME: initialize generic SPIs and McBSPs

static struct spi_board_info gta04fpga_mcspi_board_info[] = {
	// spi 4.0
	{
		.modalias	= "spidev",
		.max_speed_hz	= 48000000, //48 Mbps
		.bus_num	= 4,	// McSPI4
		.chip_select	= 0,	
		.mode = SPI_MODE_1,
	},
};

static void __init gta04fpga_init_spi(void)
{
		/* hook the spi ports to the spidev driver */
		spi_register_board_info(gta04fpga_mcspi_board_info,
			ARRAY_SIZE(gta04fpga_mcspi_board_info));
}
#endif

static struct gpio_keys_button gpio_buttons[] = {
#if 1
	{
		.code			= KEY_PHONE,
		.gpio			= AUX_BUTTON_GPIO,
		.desc			= "AUX",
		.debounce_interval = 20,
		.wakeup			= 1,
	},
#endif
#if 0
	{ /* this is a dummy entry because evdev wants to see at least one keycode in the range 0 .. 0xff to recognize a keyboard */
		.code			= KEY_POWER,	/* our real power button is controlled by the twl4030_powerbutton driver */
		.gpio			= 8,	/* GPIO8 = SYS_BOOT6 is wired to 1.8V to select the external oscillator */
		.active_low		= true,	/* never becomes active */
		.desc			= "POWER",
		.wakeup			= 1,
	},
#endif
};

static struct gpio_keys_platform_data gpio_key_info = {
	.buttons	= gpio_buttons,
	.nbuttons	= ARRAY_SIZE(gpio_buttons),
};

static struct platform_device keys_gpio = {
	.name	= "gpio-keys",
	.id	= -1,
	.dev	= {
		.platform_data	= &gpio_key_info,
	},
};

static void __init gta04_init_irq(void)
{
        if (cpu_is_omap3630())
        { // initialize different power tables
                omap2_init_common_hw(mt46h32m32lf6_sdrc_params,
                                        mt46h32m32lf6_sdrc_params,
                                        _omap37x_mpu_rate_table,
                                        _omap37x_dsp_rate_table,
                                        _omap37x_l3_rate_table);
        }
        else
        {
                omap2_init_common_hw(mt46h32m32lf6_sdrc_params,
                                        mt46h32m32lf6_sdrc_params,
                                        _omap35x_mpu_rate_table,
                                        _omap35x_dsp_rate_table,
                                        _omap35x_l3_rate_table);
        }
	omap_init_irq();
#ifdef CONFIG_OMAP_32K_TIMER
	omap2_gp_clockevent_set_gptimer(12);
#endif
	omap_gpio_init();
}

#if defined(CONFIG_HDQ_MASTER_OMAP)

static struct platform_device gta04_hdq_device = {
	.name		= "omap-hdq",
	.id			= -1,
	.dev		= {
		.platform_data	= NULL,
	},
};

#endif

#if defined(CONFIG_REGULATOR_VIRTUAL_CONSUMER)

static struct platform_device gta04_vaux1_virtual_regulator_device = {
	.name		= "reg-virt-consumer",
	.id			= 1,	
	.dev		= {
		.platform_data	= "vaux1",
	},
};

static struct platform_device gta04_vaux2_virtual_regulator_device = {
	.name		= "reg-virt-consumer",
	.id			= 2,	
	.dev		= {
		.platform_data	= "vaux2",
	},
};

#if 0
static struct platform_device gta04_vaux3_virtual_regulator_device = {
	.name		= "reg-virt-consumer",
	.id			= 3,	
	.dev		= {
		.platform_data	= "vaux3",
	},
};
#endif

static struct platform_device gta04_vaux4_virtual_regulator_device = {
	.name		= "reg-virt-consumer",
	.id			= 4,	
	.dev		= {
		.platform_data	= "vaux4",	/* allow to control VAUX4 for WLAN/Bluetooth */
	},
};

static struct platform_device gta04_vsim_virtual_regulator_device = {
	.name		= "reg-virt-consumer",
	.id			= 5,	
	.dev		= {
		.platform_data	= "vmmc_aux",	/* allow to control VSIM for GPS antenna power */
	},
};

#endif

static struct platform_device *gta04_devices[] __initdata = {
//	&leds_gpio,
	&keys_gpio,
	&gta04_dss_device,
	&gta04_cam_device,
	&gta04_bklight_device,
#if defined(CONFIG_REGULATOR_VIRTUAL_CONSUMER)
	&gta04_vaux1_virtual_regulator_device,
	&gta04_vaux2_virtual_regulator_device,
#if 0	/* camera - we enable the power through the driver */
	&gta04_vaux3_virtual_regulator_device,
#endif
	&gta04_vaux4_virtual_regulator_device,
	&gta04_vsim_virtual_regulator_device,
#endif
#if defined(CONFIG_HDQ_MASTER_OMAP)
	&gta04_hdq_device,
#endif
#if defined(CONFIG_SND_SOC_GTM601)
	&gta04_gtm601_codec_audio_device,
#endif
#if defined(CONFIG_SND_SOC_SI47XX)
	&gta04_si47xx_codec_audio_device,
#endif
#if defined(CONFIG_SND_SOC_W2CBW003)
	&gta04_w2cbw003_codec_audio_device,
#endif
};

static void __init gta04_flash_init(void)
{
	u8 cs = 0;
	u8 nandcs = GPMC_CS_NUM + 1;

	u32 gpmc_base_add = OMAP34XX_GPMC_VIRT;

	/* find out the chip-select on which NAND exists */
	while (cs < GPMC_CS_NUM) {
		u32 ret = 0;
		ret = gpmc_cs_read_reg(cs, GPMC_CS_CONFIG1);

		if ((ret & 0xC00) == 0x800) {
			printk(KERN_INFO "Found NAND on CS%d\n", cs);
			if (nandcs > GPMC_CS_NUM)
				nandcs = cs;
		}
		cs++;
	}

	if (nandcs > GPMC_CS_NUM) {
		printk(KERN_INFO "NAND: Unable to find configuration "
				 "in GPMC\n ");
		return;
	}

	if (nandcs < GPMC_CS_NUM) {
		gta04_nand_data.cs = nandcs;
		gta04_nand_data.gpmc_cs_baseaddr = (void *)
			(gpmc_base_add + GPMC_CS0_BASE + nandcs * GPMC_CS_SIZE);
		gta04_nand_data.gpmc_baseaddr = (void *) (gpmc_base_add);

		printk(KERN_INFO "Registering NAND on CS%d\n", nandcs);
		if (platform_device_register(&gta04_nand_device) < 0)
			printk(KERN_ERR "Unable to register NAND device\n");
	}
}

static struct ehci_hcd_omap_platform_data ehci_pdata __initdata = {

	/* HSUSB0 - is not a EHCI port; TPS65950 configured by twl4030.c and musb driver */
	.port_mode[0] = EHCI_HCD_OMAP_MODE_UNKNOWN,	/* HSUSB1 - n/a */
	.port_mode[1] = EHCI_HCD_OMAP_MODE_PHY,		/* HSUSB2 - USB3322C <-> WWAN */
	.port_mode[2] = EHCI_HCD_OMAP_MODE_UNKNOWN,	/* HSUSB3 - n/a */

	.phy_reset  = true,
	.reset_gpio_port[0]  = -EINVAL,
	.reset_gpio_port[1]  = 174,
	.reset_gpio_port[2]  = -EINVAL
};

static struct omap_board_mux board_mux[] __initdata = {
	{ .reg_offset = OMAP_MUX_TERMINATOR },
};

#if 0	/* use AUX button for testing if the interrupt handler works */
#undef WO3G_GPIO
#define WO3G_GPIO	AUX_BUTTON_GPIO
#endif

static irqreturn_t wake_3G_irq(int irq, void *handle)
{
	printk("3G Wakeup received :)\n");
/*	schedule_work(&work); */	
	return IRQ_HANDLED;
}

static int __init wake_3G_init(void)
{
#if defined(CONFIG_SND_SOC_GTM601)	// should be a separate config
	int err;
	
	printk("wake_3G_init() for GPIO %d\n", WO3G_GPIO);

	omap_mux_init_gpio(WO3G_GPIO, OMAP_PIN_INPUT);
	if (gpio_request(WO3G_GPIO, "3G_wakeup")) {
		printk(KERN_ERR "Failed to request GPIO %d for "
			   "3G_wakeup IRQ\n", WO3G_GPIO);
		return  -ENODEV;
	}
	
	if (gpio_direction_input(WO3G_GPIO)) {
		printk(KERN_WARNING "GPIO#%d cannot be configured as "
			   "input\n", WO3G_GPIO);
		return -ENXIO;
	}
	gpio_export(WO3G_GPIO, 0);
	omap_set_gpio_debounce(WO3G_GPIO, 1);
	omap_set_gpio_debounce_time(WO3G_GPIO, 0xa);	// means (10+1)x31 us
	set_irq_type(OMAP_GPIO_IRQ(WO3G_GPIO), IRQ_TYPE_EDGE_RISING);
	
	err = request_irq(OMAP_GPIO_IRQ(WO3G_GPIO), wake_3G_irq, 0,
					  "wake_3G", NULL /* handle */);
	if (err < 0) {
		printk(KERN_WARNING "irq %d busy?\n", OMAP_GPIO_IRQ(WO3G_GPIO));
		return err;
	}
	return 0;	
#endif
}

static void __init gta04_init(void)
{
	/* this switches most pins to safe mode as defined in arch/arm/mach-omap2/mux34xx.c */
	/* but this may also be harmful if the Pins have built-in PullUp or PullDown states */
	
	omap3_mux_init(board_mux, OMAP_PACKAGE_CBB);
	
	gta04_init_rev();
	gta04_i2c_init();
	platform_add_devices(gta04_devices,
						 ARRAY_SIZE(gta04_devices));
	omap_serial_init();
	
	// for a definition of the mux names see arch/arm/mach-omap2/mux34xx.c
	// the syntax of the first paramter to omap_mux_init_signal() is "muxname" or "m0name.muxname" (for ambiguous modes)
	// note: calling omap_mux_init_signal() can modify the parameter string (replace '.' by '\0')
	
	omap_mux_init_signal("mcbsp3_clkx.uart2_tx", OMAP_PIN_OUTPUT);	// gpio 142 / GPS TX
	omap_mux_init_signal("mcbsp3_fsx.uart2_rx", OMAP_PIN_INPUT);	// gpio 143 / GPS RX
#ifdef CONFIG_PANEL_SHARP_LQ070Y3DG3B	// GTA04b3 board
	omap_mux_init_gpio(20, OMAP_PIN_INPUT_PULLDOWN);	// gpio 20
#endif

	printk(KERN_INFO "Revision GTA04A%d\n", gta04_version);
	
#if defined(CONFIG_KEYBOARD_TCA8418) || defined(CONFIG_KEYBOARD_TCA8418_MODULE)
	{ /* dynamically insert the correct IRQ number */
	int i;
	for(i=0; i<ARRAY_SIZE(gta04_i2c2_boardinfo); i++)
		if(strcmp(gta04_i2c2_boardinfo[i].type, "tca8418") == 0)
			gta04_i2c2_boardinfo[i].irq = KEYIRQ_GPIO;
	}
	omap_mux_init_gpio(KEYIRQ_GPIO, OMAP_PIN_INPUT_PULLUP);	// gpio 10 or 176

	if (gpio_request(KEYIRQ_GPIO, "keyirq")) {
		printk(KERN_ERR "Failed to request GPIO %d for "
			   "KEYIRQ\n", KEYIRQ_GPIO);
	}
	
	if (gpio_direction_input(KEYIRQ_GPIO)) {
		printk(KERN_WARNING "GPIO#%d cannot be configured as "
			   "input\n", KEYIRQ_GPIO);
	}
	omap_set_gpio_debounce(KEYIRQ_GPIO, 1);
	omap_set_gpio_debounce_time(KEYIRQ_GPIO, 0xa);
	set_irq_type(OMAP_GPIO_IRQ(KEYIRQ_GPIO), IRQ_TYPE_EDGE_FALLING);
#endif
	
	// gpio_export() allows to access through /sys/devices/virtual/gpio/gpio*/value
		
	gpio_request(145, "GPS_ON");
	gpio_direction_output(145, false);
	gpio_export(145, 0);	// no direction change by user
	
	// should be a backlight driver using PWM
	gpio_request(57, "LCD_BACKLIGHT");
	gpio_direction_output(57, true);
	gpio_export(57, 0);	// no direction change
	
	// omap_mux_init_gpio(138, OMAP_PIN_INPUT);	// gpio 138 - with no pullup/pull-down
	gpio_request(144, "EXT_ANT");
	gpio_direction_input(144);
	gpio_export(144, 0);	// no direction change
	
	if(gta04_version >= 3)
		{
		// enable AUX out/Headset switch
		gpio_request(55, "AUX_OUT");	// enable headset out
		gpio_direction_output(55, true);
		gpio_export(55, 0);	// no direction change
	
		// disable Video out switch
		gpio_request(23, "VIDEO_OUT");	// enable video out
		gpio_direction_output(23, false);
		gpio_export(23, 0);	// no direction change
		
		}

	gpio_request(13, "RS232");	// enable RS232/disable IrDA
	gpio_direction_output(13, 1);	// keep RS232 enabled
	gpio_export(13, 0);	// no direction change
		
	gpio_request(21, "RS232-EXT");	// control EXT line/IrDA FIR-SEL
	gpio_direction_output(21, 1);
	gpio_export(21, 0);	// no direction change by user

	if(gta04_version >= 4) { /* feature of GTA04A4 */
		omap_mux_init_gpio(186, OMAP_PIN_OUTPUT);	// this needs CONFIG_OMAP_MUX!
		gpio_request(186, "WWAN_RESET");
		gpio_direction_output(186, 0);		// keep initial value 0
		gpio_export(186, 0);	// no direction change
	}
	
	// changed: don't wake up the modem automatically on GTA04A4! Leave that to user space or some rfkill control

#if 0 && defined(CONFIG_SND_SOC_GTM601)	// should be a separate config
	if(gta04_version >= 4)
		{ // trigger ON_KEY so that Modem should initialize now and respond on the internal USB (EHCI)

			printk("GTM601W wake up\n");
			// 1. reset state of GPIO186 is low (ON_KEY not pressed)
		
			printk("ON_KEY is initially released\n");
			
			// 2. activate for 250 ms to switch module on
			
			gpio_set_value(186, 1);	// press button
			printk("ON_KEY pressed\n");
			mdelay(250);
			gpio_set_value(186, 0);	// release button
			printk("ON_KEY released\n");
			
			// FIXME: install driver in pm_power_off
			// that activates ON_KEY for at least 200 ms

#if 0	// does not work as intended
			// 3. deactivate for 50ms - this will be ignored if we just switched on
			// but it will be treated as the real wakeup if the modem was still on
			
			mdelay(50);
			
			// 4. activate for 250 ms to switch definitively on
			
			gpio_set_value(186, 1);	// press button
			printk("ON_KEY pressed\n");
			mdelay(250);
			gpio_set_value(186, 0);	// release button
			printk("ON_KEY released\n");
			
			// 5. deactivate for 5000ms - this allows the module to initialize
			// and wait for a power-off signal (otherwise it is ignored)

			mdelay(5000);
			
			// 6. activate again to prepare for automagic module-off on CPU power-off (GPIO goes to low by hardware)
			gpio_set_value(186, 1);	// press button again in preparation for a release
			printk("ON_KEY pressed\n");
#endif
		}
#endif
	
	usb_musb_init();
	usb_ehci_init(&ehci_pdata);
	gta04_flash_init();
	
	/* Ensure SDRC pins are mux'd for self-refresh */
	omap_mux_init_signal("sdrc_cke0", OMAP_PIN_OUTPUT);
	omap_mux_init_signal("sdrc_cke1", OMAP_PIN_OUTPUT);
	
	/* TPS65950 mSecure initialization for write access enabling to RTC registers */
	omap_mux_init_gpio(TWL4030_MSECURE_GPIO, OMAP_PIN_OUTPUT);	// this needs CONFIG_OMAP_MUX!
	gpio_request(TWL4030_MSECURE_GPIO, "mSecure");
	gpio_direction_output(TWL4030_MSECURE_GPIO, true);
	
	gta04_display_init();
	regulator_has_full_constraints();
	wake_3G_init();
}

static void __init gta04_map_io(void)
{
	omap2_set_globals_343x();
	omap2_map_common_io();
}

MACHINE_START(GTA04, "GTA04")
	/* Maintainer: Nikolaus Schaller - http://www.gta04.org */
	.phys_io	= 0x48000000,
	.io_pg_offst	= ((0xfa000000) >> 18) & 0xfffc,
	.boot_params	= 0x80000100,
	.map_io		= gta04_map_io,
	.init_irq	= gta04_init_irq,
	.init_machine	= gta04_init,
	.timer		= &omap_timer,
MACHINE_END
