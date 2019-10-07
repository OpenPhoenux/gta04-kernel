/*
 * BQ24296/7 battery charger and OTG regulator
 *
 * found in some Android driver and hacked by <hns@goldelico.com>
 * to become useable for the Pyra
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 */

#define UNUSED 0
#define FIXME 0

#include <linux/module.h>
#include <linux/param.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/idr.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>

#define VSYS_REGULATOR	0
#define OTG_REGULATOR	1
#define NUM_REGULATORS	2

/* I2C register define */
#define INPUT_SOURCE_CONTROL_REGISTER		0x00
#define POWER_ON_CONFIGURATION_REGISTER		0x01
#define CHARGE_CURRENT_CONTROL_REGISTER		0x02
#define PRE_CHARGE_TERMINATION_CURRENT_CONTROL_REGISTER		0x03
#define CHARGE_VOLTAGE_CONTROL_REGISTER		0x04
#define TERMINATION_TIMER_CONTROL_REGISTER		0x05
#define THERMAL_REGULATION_CONTROL_REGISTER		0x06
#define MISC_OPERATION_CONTROL_REGISTER		0x07
#define SYSTEM_STATS_REGISTER		0x08
#define FAULT_STATS_REGISTER		0x09
#define VENDOR_STATS_REGISTER		0x0A

/* REG00 input source control register value */
#define EN_HIZ_ENABLE	 1
#define EN_HIZ_DISABLE	 0
#define EN_HIZ_OFFSET	 7
#define EN_HIZ_MASK	 1

#define VINDPM_OFFSET		3
#define VINDPM_MASK		0xf

#define IINLIM_100MA		0
#define IINLIM_150MA		1
#define IINLIM_500MA		2
#define IINLIM_900MA		3
#define IINLIM_1200MA		4
#define IINLIM_1500MA		5
#define IINLIM_2000MA		6
#define IINLIM_3000MA		7
#define IINLIM_OFFSET		0
#define IINLIM_MASK		7

/* REG01 power-on configuration register value */
/* OTG Mode Current Config */
#define OTG_MODE_CURRENT_CONFIG_500MA		0x00
#define OTG_MODE_CURRENT_CONFIG_1300MA	0x01
#define OTG_MODE_CURRENT_CONFIG_OFFSET	0
#define OTG_MODE_CURRENT_CONFIG_MASK		0x01

/* VSYS Minimum */
#define SYS_MIN_OFFSET		1
#define SYS_MIN_MASK		0x7

/* Charge Mode Config */
#define CHARGE_MODE_CONFIG_CHARGE_DISABLE	0x00
#define CHARGE_MODE_CONFIG_CHARGE_BATTERY	0x01
#define CHARGE_MODE_CONFIG_OTG_OUTPUT		0x02
#define CHARGE_MODE_CONFIG_OFFSET		4
#define CHARGE_MODE_CONFIG_MASK		0x03

/* Watchdog */
#define WATCHDOG_RESET	0x40

/* Reset */
#define REGISTER_RESET_ENABLE	 1
#define REGISTER_RESET_DISABLE	 0
#define REGISTER_RESET_OFFSET	 7
#define REGISTER_RESET_MASK	 1

/* REG02 charge current limit register value */
#define CHARGE_CURRENT_64MA		0x01
#define CHARGE_CURRENT_128MA		0x02
#define CHARGE_CURRENT_256MA		0x04
#define CHARGE_CURRENT_512MA		0x08
#define CHARGE_CURRENT_1024MA		0x10
#define CHARGE_CURRENT_1536MA		0x18
#define CHARGE_CURRENT_2048MA		0x20
#define CHARGE_CURRENT_OFFSET		2
#define CHARGE_CURRENT_MASK		0x3f

/* REG03 Pre-Charge/Termination Current Control Register value */
/* Pre-Charge Current Limit */
#define PRE_CHARGE_CURRENT_LIMIT_128MA		0x00
#define PRE_CHARGE_CURRENT_LIMIT_256MA		0x01
#define PRE_CHARGE_CURRENT_LIMIT_OFFSET		4
#define PRE_CHARGE_CURRENT_LIMIT_MASK		0x0f
/* Termination Current Limit */
#define TERMINATION_CURRENT_LIMIT_128MA		0x00
#define TERMINATION_CURRENT_LIMIT_256MA		0x01
#define TERMINATION_CURRENT_LIMIT_OFFSET		0
#define TERMINATION_CURRENT_LIMIT_MASK		0x0f

/* REG04 Charge Voltage Register */
#define VREG_MASK	0x3f
#define VREG_OFFSET	2

/* REG05 Charge Termination/Timer control register value */
#define WATCHDOG_DISABLE		0
#define WATCHDOG_40S		1
#define WATCHDOG_80S		2
#define WATCHDOG_160S		3
#define WATCHDOG_OFFSET		4
#define WATCHDOG_MASK		3

/* REG06 boost voltage/thermal regulation register */
#define BOOSTV_OFFSET	4
#define BOOSTV_MASK	0xf

/* REG07 misc operation control register value */
#define DPDM_ENABLE	 1
#define DPDM_DISABLE	 0
#define DPDM_OFFSET	 7
#define DPDM_MASK	 1

/* REG08 system status register value */
#define VBUS_UNKNOWN		0
#define VBUS_USB_HOST		1
#define VBUS_ADAPTER_PORT		2
#define VBUS_OTG		3
#define VBUS_OFFSET		6
#define VBUS_MASK		3

#define CHRG_NO_CHARGING		0
#define CHRG_PRE_CHARGE		1
#define CHRG_FAST_CHARGE		2
#define CHRG_CHRGE_DONE		3
#define CHRG_OFFSET		4
#define CHRG_MASK		3

#define DPM_STAT	0x08
#define PG_STAT		0x04
#define THERM_STAT	0x02
#define VSYS_STAT	0x01

/* REG09 fault status register value */

#define WATCHDOG_FAULT	0x80
#define OTG_FAULT	0x40
#define CHRG_FAULT_OFFSET	4
#define CHRG_FAULT_MASK	0x3
#define BAT_FAULT	0x08
#define NTC_FAULT_OFFSET	0
#define NTC_FAULT_MASK	0x3

/* REG0a vendor status register value */
/* #define CHIP_BQ24190		0
#define CHIP_BQ24191		1
#define CHIP_BQ24192		2
#define CHIP_BQ24192I		3
#define CHIP_BQ24190_DEBUG		4
#define CHIP_BQ24192_DEBUG		5 */
#define CHIP_BQ24296		1
#define CHIP_BQ24297		3
#define CHIP_MP2624		0
#define CHIP_OFFSET		2
#define CHIP_MASK		7

#define BQ24296_CHG_COMPELET       0x03
#define BQ24296_NO_CHG             0x00

#define BQ24296_DC_CHG             0x02
#define BQ24296_USB_CHG            0x01

struct bq24296_board {
	struct gpio_desc *dc_det_pin;
	struct gpio_desc *psel_pin;
	unsigned int chg_current[3];
};

struct bq24296_device_info {
	struct device 		*dev;
	struct i2c_client	*client;
	const struct i2c_device_id *id;

	struct mutex	var_lock;

	struct delayed_work	usb_detect_work;
	struct work_struct	irq_work;
	struct workqueue_struct *workqueue;

	struct regulator_desc desc[NUM_REGULATORS];
	struct device_node *of_node[NUM_REGULATORS];
	struct regulator_dev *rdev[NUM_REGULATORS];
	struct regulator_init_data *pmic_init_data;

	struct gpio_desc *dc_det_pin;
	struct gpio_desc *psel_pin;

	u8 r8, r9;	/* status register values from last read */
	u8 prev_r8, prev_r9;
	bool adapter_plugged;	/* is power adapter plugged in */

	u8 chg_current;
	u8 usb_input_current;
	u8 adp_input_current;

	struct power_supply *usb;
};

/* should be read from DT properties! */
static unsigned int battery_voltage_max_design_uV = 4200000;	// default
static unsigned int max_VSYS_uV = 5000000;	// "unlimited"

#if 0
#define DBG(x...) printk(KERN_INFO x)
#define DEBUG 1
#else
#define DBG(x...) do { } while (0)
#endif

/*
 * Common code for BQ24296 devices read
 */
static int bq24296_i2c_reg8_read(const struct i2c_client *client, const char reg, char *buf, int count)
{
	struct i2c_adapter *adap=client->adapter;
	struct i2c_msg msgs[2];
	int ret;
	char reg_buf = reg;

	msgs[0].addr = client->addr;
	msgs[0].flags = client->flags;
	msgs[0].len = 1;
	msgs[0].buf = &reg_buf;

	msgs[1].addr = client->addr;
	msgs[1].flags = client->flags | I2C_M_RD;
	msgs[1].len = count;
	msgs[1].buf = (char *)buf;

	ret = i2c_transfer(adap, msgs, 2);

	return (ret == 2)? count : ret;
}

static int bq24296_i2c_reg8_write(const struct i2c_client *client, const char reg, const char *buf, int count)
{
	struct i2c_adapter *adap=client->adapter;
	struct i2c_msg msg;
	int ret;
	char *tx_buf = (char *)kmalloc(count + 1, GFP_KERNEL);

	if(!tx_buf)
		return -ENOMEM;
	tx_buf[0] = reg;
	memcpy(tx_buf+1, buf, count);

	msg.addr = client->addr;
	msg.flags = client->flags;
	msg.len = count + 1;
	msg.buf = (char *)tx_buf;

	ret = i2c_transfer(adap, &msg, 1);
	kfree(tx_buf);
	return (ret == 1) ? count : ret;
}

static inline int bq24296_read(struct i2c_client *client, u8 reg, u8 buf[], unsigned len)
{
	int ret;

	ret = bq24296_i2c_reg8_read(client, reg, buf, len);
	return ret;
}

static inline int bq24296_write(struct i2c_client *client, u8 reg, u8 const buf[], unsigned len)
{
	int ret;

#if 0	// debug and disable write commands
	printk("bq24296_write %02x: %02x\n", reg, *buf);
	return 0;
#endif
	ret = bq24296_i2c_reg8_write(client, reg, buf, (int)len);
	return ret;
}

static int bq24296_update_reg(struct i2c_client *client, int reg, u8 value, u8 mask )
{
	int ret =0;
	u8 retval = 0;

	ret = bq24296_read(client, reg, &retval, 1);
	if (ret < 0) {
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);
		return ret;
	}
	ret = 0;

#if 0
	printk("bq24296_update_reg %02x: ( %02x & %02x ) | %02x -> %02x\n", reg, retval, (u8) ~mask, value, (u8) ((retval & ~mask) | value));
#endif

	if ((retval & mask) != value) {
		retval = (retval & ~mask) | value;
		ret = bq24296_write(client, reg, &retval, 1);
		if (ret < 0) {
			dev_err(&client->dev, "%s: err %d\n", __func__, ret);
			return ret;
		}
		ret = 0;
	}
#if 0	// DEBUG
{
	int i;
	u8 buffer;
	for(i=0;i<11;i++)
		{
		bq24296_read(client, i, &buffer, 1);
		printk("  reg %02x value %02x\n", i, buffer);
		}
}
#endif

	return ret;
}

/* sysfs tool to show all register values */

static ssize_t show_registers(struct device *dev, struct device_attribute *attr, char *buf)
{
	int i;
	u8 buffer;
	struct i2c_client *client = to_i2c_client(dev);
	struct bq24296_device_info *di = i2c_get_clientdata(client);
	int len = 0;

	for(i=0;i<11;i++)
		{
		int n;
		bq24296_read(di->client,i,&buffer,1);
		n=scnprintf(buf, 256, "reg %02x value %02x\n", i, buffer);
		buf += n;
		len += n;
		}
	return len;
}

DEVICE_ATTR(registers, 0444, show_registers,NULL);

// FIXME: review very critical what we need to initialize
// and why it is constant and not defined by device tree properties!

static int bq24296_init_registers(struct bq24296_device_info *di)
{
	int ret = 0;
	int max_uV, bits;

#if 0	// NO: don't do that because we are powered through this chip - u-boot must have initialized properly
	/* reset the configuration register */
	 ret = bq24296_update_reg(di->client,
				 POWER_ON_CONFIGURATION_REGISTER,
				 REGISTER_RESET_ENABLE << REGISTER_RESET_OFFSET,
				 REGISTER_RESET_MASK << REGISTER_RESET_OFFSET);
	if (ret < 0) {
		dev_err(&di->client->dev, "%s(): Failed to reset the register \n",
		__func__);
	goto final;
	}

	mdelay(5);	/* maybe we should poll */
#endif

/*
 * FIXME: why do we want to disable the watchdog?
 *
 * U-Boot has it already disabled because it can't poll the chip if waiting for commands
 * on the command-line and while Linux starts
 *
 * But in Linux we should better re-enable it and reset the watchdog by our polling function
 * Make it a DT property "ti,enable-watchdog"! - there should be the option to leave it as defined by U-Boot
 *
 * Probably U-Boot should re-enable it before launching Linux so it becomes active
 * even if Linux does not properly boot
 *
 * So we should only restart if every now and then
 */

#if 0
	/* Disable the watchdog */
	ret = bq24296_update_reg(di->client,
				  TERMINATION_TIMER_CONTROL_REGISTER,
				  WATCHDOG_DISABLE << WATCHDOG_OFFSET,
				  WATCHDOG_MASK << WATCHDOG_OFFSET);
	if (ret < 0) {
		dev_err(&di->client->dev, "%s(): Failed to disable the watchdog \n",
				__func__);
		goto final;
	}
#endif

	/* Set Pre-Charge Current Limit as 128mA */
	ret = bq24296_update_reg(di->client,
				  PRE_CHARGE_TERMINATION_CURRENT_CONTROL_REGISTER,
				  PRE_CHARGE_CURRENT_LIMIT_128MA << PRE_CHARGE_CURRENT_LIMIT_OFFSET,
				  PRE_CHARGE_CURRENT_LIMIT_MASK << PRE_CHARGE_CURRENT_LIMIT_OFFSET);
	if (ret < 0) {
		dev_err(&di->client->dev, "%s(): Failed to set pre-charge limit 128mA\n",
				__func__);
		goto final;
	}

	/* Set Termination Current Limit as 128mA */
	ret = bq24296_update_reg(di->client,
				  PRE_CHARGE_TERMINATION_CURRENT_CONTROL_REGISTER,
				  TERMINATION_CURRENT_LIMIT_128MA << TERMINATION_CURRENT_LIMIT_OFFSET,
				  TERMINATION_CURRENT_LIMIT_MASK << TERMINATION_CURRENT_LIMIT_OFFSET);
	if (ret < 0) {
		dev_err(&di->client->dev, "%s(): Failed to set termination limit 128mA\n",
				__func__);
		goto final;
	}

	// VSYS may be 150mV above fully charged battery voltage
	// so to effectively limit VSYS we may have to lower the max. battery voltage
	// this may be switched to 100mV for the mps,mp2624

	max_uV = min(max_VSYS_uV - 150000, battery_voltage_max_design_uV);

	bits = (max_uV - 3504000) / 16000;
	bits = max(bits, 0);
	bits = min(bits, 63);

	dev_info(&di->client->dev, "%s(): translated vbatt_max=%u and VSYS_max=%u to VREG=%u (%02x)\n",
		__func__,
		battery_voltage_max_design_uV, max_VSYS_uV, max_uV,
		bits);

	ret = bq24296_update_reg(di->client,
				  CHARGE_VOLTAGE_CONTROL_REGISTER,
				  bits << VREG_OFFSET,
				  VREG_MASK << VREG_OFFSET);
	if (ret < 0) {
		dev_err(&di->client->dev, "%s(): Failed to set max. battery voltage\n",
				__func__);
		goto final;
	}

#if 0
	/* Set System Voltage Limit as 3.2V */
	/* FIXME: read from DT */
	ret = bq24296_update_reg(di->client,
				  POWER_ON_CONFIGURATION_REGISTER,
				  0x04,	/* 3.0V + 0.2V */
				  SYS_MIN_MASK << SYS_MIN_OFFSET);
	if (ret < 0) {
		dev_err(&di->client->dev, "%s(): Failed to set voltage limit 3.2V \n",
				__func__);
		goto final;
	}
#endif

#if 0
	/* disable boost temperature protection (for debugging) */
	ret = bq24296_update_reg(di->client,
				 THERMAL_REGULATION_CONTROL_REGISTER,
				 0x0c,	/* BHOT[1:0]=11 */
				 0x0c);
	if (ret < 0) {
		dev_err(&di->client->dev, "%s(): Failed to disable boost temperature monitor \n",
				__func__);
		goto final;
	}
#endif

final:
	return ret;
}

/* helper functions */

static int bq24296_limit_current_mA_to_bits(int mA)
{
	u8 data;
	if (mA < 120)
		data = 0;
	else if(mA < 400)
		data = 1;
	else if(mA < 700)
		data = 2;
	else if(mA < 1000)
		data = 3;
	else if(mA < 1200)
		data = 4;
	else if(mA < 1800)
		data = 5;
	else if(mA < 2200)
		data = 6;
	else
		data = 7;
	return data;
}

static int bq24296_chg_current_mA_to_bits(int mA)
{
	u8 data;

	data = (mA)/64;
	data &= 0xff;
	return data;
}

/* getter and setter functions */

static int bq24296_get_vindpm_uV(struct bq24296_device_info *di)
{
	int ret;
	u8 retval = 0;

	ret = bq24296_read(di->client, INPUT_SOURCE_CONTROL_REGISTER, &retval, 1);
	if (ret < 0) {
		dev_err(&di->client->dev, "%s: err %d\n", __func__, ret);
		return ret;
	}

	return 3880000 + 80000*((retval >> VINDPM_OFFSET) & VINDPM_MASK);
}

static const unsigned int iinlim_table[] = {
	100000,
	150000,
	500000,
	900000,
	1000000,
	1500000,
	2000000,
	3000000,
};

static int bq24296_input_current_limit_uA(struct bq24296_device_info *di)
{
	int ret;
	u8 retval = 0;

	ret = bq24296_read(di->client, INPUT_SOURCE_CONTROL_REGISTER, &retval, 1);
	if (ret < 0) {
		dev_err(&di->client->dev, "%s: err %d\n", __func__, ret);
		return ret;
	}

	if(((retval >> EN_HIZ_OFFSET) & EN_HIZ_MASK) == EN_HIZ_ENABLE)
		return 0;	// High-Z state

	return iinlim_table[(retval >> IINLIM_OFFSET) & IINLIM_MASK];
}

static int bq24296_update_input_current_limit(struct bq24296_device_info *di, int value)
{
	int ret = 0;
	u8 hiz = (value < 0 ? EN_HIZ_ENABLE : EN_HIZ_DISABLE);

printk("bq24296_update_input_current_limit(%d)\n", value);

	ret = bq24296_update_reg(di->client,
				  INPUT_SOURCE_CONTROL_REGISTER,
				  (((value & IINLIM_MASK) << IINLIM_OFFSET) | (hiz << EN_HIZ_OFFSET)),
				  ((IINLIM_MASK << IINLIM_OFFSET) | (EN_HIZ_MASK << EN_HIZ_OFFSET)));
	if (ret < 0) {
		dev_err(&di->client->dev, "%s(): Failed to set input current limit (0x%x) \n",
				__func__, value);
	}

	return ret;
}

static int bq24296_set_charge_current(struct bq24296_device_info *di, u8 value)
{
	int ret = 0;

	ret = bq24296_update_reg(di->client,
				  CHARGE_CURRENT_CONTROL_REGISTER,
				  (value << CHARGE_CURRENT_OFFSET) ,(CHARGE_CURRENT_MASK <<CHARGE_CURRENT_OFFSET ));
	if (ret < 0) {
		dev_err(&di->client->dev, "%s(): Failed to set charge current limit (0x%x) \n",
				__func__, value);
	}
	return ret;
}

static int bq24296_update_en_hiz_disable(struct bq24296_device_info *di)
{
	int ret = 0;

	ret = bq24296_update_reg(di->client,
				  INPUT_SOURCE_CONTROL_REGISTER,
				  EN_HIZ_DISABLE << EN_HIZ_OFFSET,
				  EN_HIZ_MASK << EN_HIZ_OFFSET);
	if (ret < 0) {
		dev_err(&di->client->dev, "%s(): Failed to set en_hiz_disable\n",
				__func__);
	}
	return ret;
}

int bq24296_set_input_current(struct bq24296_device_info *di, int on)
{
	bq24296_update_input_current_limit(di, on ? IINLIM_3000MA : IINLIM_500MA);
	DBG("bq24296_set_input_current %s\n", on ? "3000mA" : "500mA");

	return 0;
}

static int bq24296_update_charge_mode(struct bq24296_device_info *di, u8 value)
{
	int ret = 0;

	ret = bq24296_update_reg(di->client,
				  POWER_ON_CONFIGURATION_REGISTER,
				  value << CHARGE_MODE_CONFIG_OFFSET,
				  CHARGE_MODE_CONFIG_MASK << CHARGE_MODE_CONFIG_OFFSET);
	if (ret < 0) {
		dev_err(&di->client->dev, "%s(): Failed to set charge mode(0x%x) \n",
				__func__, value);
	}

	return ret;
}

static int bq24296_update_otg_mode_current(struct bq24296_device_info *di, u8 value)
{
	int ret = 0;

	ret = bq24296_update_reg(di->client,
				  POWER_ON_CONFIGURATION_REGISTER,
				  value << OTG_MODE_CURRENT_CONFIG_OFFSET,
				  OTG_MODE_CURRENT_CONFIG_MASK << OTG_MODE_CURRENT_CONFIG_OFFSET);
	if (ret < 0) {
		dev_err(&di->client->dev, "%s(): Failed to set otg current mode(0x%x) \n",
				__func__, value);
	}
	return ret;
}

#ifdef UNUSED
static int bq24296_charge_mode_config(struct bq24296_device_info *di, bool on)
{
	if (on) {
		bq24296_update_en_hiz_disable(di);
		mdelay(5);
		bq24296_update_charge_mode(di, CHARGE_MODE_CONFIG_OTG_OUTPUT);
		mdelay(10);
		bq24296_update_otg_mode_current(di, OTG_MODE_CURRENT_CONFIG_1300MA);
	}
	else {
		bq24296_update_charge_mode(di, CHARGE_MODE_CONFIG_CHARGE_BATTERY);
	}

	DBG("bq24296_charge_mode_config is %s\n", on ? "OTG Mode" : "Charge Mode");

	return 0;
}
#endif

static inline bool bq24296_battery_present(struct bq24296_device_info *di)
{ /* assume there is no battery if there is an NTC fault */
	return ((di->r9 >> NTC_FAULT_OFFSET) & NTC_FAULT_MASK) == 0;	/* if no fault */
}

static inline bool bq24296_input_present(struct bq24296_device_info *di)
{ /* VBUS is available */
	return (di->r8 & PG_STAT) != 0;
}

static void bq2429x_input_available(struct bq24296_device_info *di, bool state)
{ /* track external power input state and trigger actions on change */
	if (state && !di->adapter_plugged) {
		di->adapter_plugged = true;

		DBG("bq24296: VBUS became available\n");
		printk("bq24296: VBUS became available\n");

// FIXME: send power status changed notifier

		// this should have been queried/provided by the USB stack...
		bq24296_update_input_current_limit(di, di->usb_input_current);

		/* start charging */
		if (bq24296_battery_present(di))
			bq24296_update_charge_mode(di, CHARGE_MODE_CONFIG_CHARGE_BATTERY);
		}
	else if (!state && di->adapter_plugged) {
		di->adapter_plugged = false;

// FIXME: send power status changed notifier

		DBG("bq24296: VBUS became unavailable\n");
		printk("bq24296: VBUS became unavailable\n");
		}
}

static int bq24296_usb_detect(struct bq24296_device_info *di)
{
	int ret;

	mutex_lock(&di->var_lock);	/* if interrupt and polling occur at the same time */

//	printk("%s, line=%d\n", __func__,__LINE__);

	ret = bq24296_read(di->client, SYSTEM_STATS_REGISTER, &di->r8, 1);
	if (ret != 1) {
		mutex_unlock(&di->var_lock);

		dev_err(&di->client->dev, "%s: err %d\n", __func__, ret);

		return ret;
	}

	ret = bq24296_read(di->client, FAULT_STATS_REGISTER, &di->r9, 1);
	if (ret != 1) {
		mutex_unlock(&di->var_lock);

		dev_err(&di->client->dev, "%s: err %d\n", __func__, ret);

		return ret;
	}

#if 1
	{ // print changes to last state
		if (di->r8 != di->prev_r8 || di->r9 != di->prev_r9)
			{
			char string[200];
			sprintf(string, "r8=%02x", di->r8);
			switch ((di->r8>>6)&3) {
				case 1: strcat(string, " HOST"); break;
				case 2: strcat(string, " ADAP"); break;
				case 3: strcat(string, " OTG"); break;
			};
			switch ((di->r8>>4)&3) {
				case 1: strcat(string, " PRECHG"); break;
				case 2: strcat(string, " FCHG"); break;
				case 3: strcat(string, " CHGTERM"); break;
			};
			if ((di->r8>>3)&1) strcat(string, " INDPM");
			if ((di->r8>>2)&1) strcat(string, " PWRGOOD");
			if ((di->r8>>1)&1) strcat(string, " THERMREG");
			if ((di->r8>>0)&1) strcat(string, " VSYSMIN");
			sprintf(string+strlen(string), " r9=%02x", di->r9);
			if ((di->r9>>7)&1) strcat(string, " WDOG");
			if ((di->r9>>6)&1) strcat(string, " OTGFAULT");
			switch ((di->r9>>4)&3) {
				case 1: strcat(string, " UNPLUG"); break;
				case 2: strcat(string, " THERMAL"); break;
				case 3: strcat(string, " CHGTIME"); break;
			};
			if ((di->r9>>3)&1) strcat(string, " BATFAULT");
			if ((di->r9>>2)&1) strcat(string, " RESERVED");
			if ((di->r9>>1)&1) strcat(string, " COLD");
			if ((di->r9>>0)&1) strcat(string, " HOT");
			printk("%s: %s\n", __func__, string);
			}
		di->prev_r8 = di->r8, di->prev_r9 = di->r9;
	}
#endif


#if FIXME

/*
 * we should also reset the watchdog every now and then
 * at least if we run from battery
 * If watchdog operation is with battery only, we should
 * enable/disable it on demand
 */

#endif

	if (di->dc_det_pin){
#ifdef UNUSED
		/* detect extermal charging request */
		ret = gpiod_get_value(di->dc_det_pin);
		if (ret == 0) {
			bq24296_update_input_current_limit(di, di->adp_input_current);
			bq24296_set_charge_current(di, CHARGE_CURRENT_2048MA);
			bq24296_charge_mode_config(di, 0);
		}
		else {
			bq24296_update_input_current_limit(di, IINLIM_500MA);
			bq24296_set_charge_current(di, CHARGE_CURRENT_512MA);
		}
		DBG("%s: di->dc_det_pin=%x\n", __func__, ret);
#endif
	}
	else {
#ifdef OLD
		DBG("%s: dwc_otg_check_dpdm %d\n", __func__, dwc_otg_check_dpdm(0));
		switch(dwc_otg_check_dpdm(0)) {
			case 2: // USB Wall charger
				bq24296_update_input_current_limit(di->usb_input_current);
				bq24296_set_charge_current(CHARGE_CURRENT_2048MA);
				bq24296_charge_mode_config(0);
				DBG("bq24296: detect usb wall charger\n");
				break;
			case 1: //normal USB
				if (0 == get_gadget_connect_flag()){  // non-standard AC charger
					bq24296_update_input_current_limit(di->usb_input_current);
					bq24296_set_charge_current(CHARGE_CURRENT_2048MA);
					bq24296_charge_mode_config(0);;
				}else
					{
					// connect to pc
					bq24296_update_input_current_limit(di->usb_input_current);
					bq24296_set_charge_current(CHARGE_CURRENT_512MA);
					bq24296_charge_mode_config(0);
					DBG("bq24296: detect normal usb charger\n");
					}
				break;
			default:
				bq24296_update_input_current_limit(IINLIM_500MA);
				bq24296_set_charge_current(CHARGE_CURRENT_512MA);
				DBG("bq24296: detect no usb\n");
				break;
#else

/* FIXME/CHECKME:
   do we really have to actively switch to charging or does the charger start automatically?
   Then, we might not even need this scheduled worker function

   We should send an udev event like other chargers do

*/

		/* handle (momentarily) disconnect of VBUS */
		if ((di->r9 >> CHRG_FAULT_OFFSET) & CHRG_FAULT_MASK)
			bq2429x_input_available(di, false);

		/* since we are polling slowly VBUS may already be back again */
		bq2429x_input_available(di, bq24296_input_present(di));

#endif
	}

	mutex_unlock(&di->var_lock);

	return 0;
}

/* polling */

static void usb_detect_work_func(struct work_struct *wp)
{
	struct delayed_work *dwp = (struct delayed_work *)container_of(wp, struct delayed_work, work);
	struct bq24296_device_info *di = (struct bq24296_device_info *)container_of(dwp, struct bq24296_device_info, usb_detect_work);
	int ret;

	ret = bq24296_usb_detect(di);

	if (ret == 0)
		schedule_delayed_work(&di->usb_detect_work, 1*HZ);
}

/* interrupt */

static void bq2729x_irq_work_func(struct work_struct *wp)
{
	struct bq24296_device_info *di = (struct bq24296_device_info *)container_of(wp, struct bq24296_device_info, irq_work);

	printk("%s: di = %px\n", __func__, di);

	bq24296_usb_detect(di);

	printk("%s: r8=%02x r9=%02x\n", __func__, di->r8, di->r9);
}

static irqreturn_t bq2729x_chg_irq_func(int irq, void *dev_id)
{
	struct bq24296_device_info *info = dev_id;
	DBG("%s\n", __func__);
	printk("%s\n", __func__);

	queue_work(info->workqueue, &info->irq_work);

	return IRQ_HANDLED;
}

/* regulator framework integration for VSYS and OTG */

static const unsigned int vsys_VSEL_table[] = {
	3000000,
	3100000,
	3200000,
	3300000,
	3400000,
	3500000,
	3600000,
	3700000,
};

static const unsigned int otg_VSEL_table[] = {
	4550000,
	4614000,
	4678000,
	4742000,
	4806000,
	4870000,
	4934000,
	4998000,
	5062000,
	5126000,
	5190000,
	5254000,
	5318000,
	5382000,
	5446000,
	5510000,
};

static int bq24296_get_vsys_voltage(struct regulator_dev *dev)
{
	struct bq24296_device_info *di = rdev_get_drvdata(dev);
	int idx = dev->desc->id;
	int ret;
	u8 retval;

	printk("bq24296_get_vsys_voltage(%d)\n", idx);

	ret = bq24296_read(di->client, POWER_ON_CONFIGURATION_REGISTER, &retval, 1);
	if (ret < 0)
		return ret;
	printk(" => %d uV\n", vsys_VSEL_table[(retval >> SYS_MIN_OFFSET) & SYS_MIN_MASK]);
	return vsys_VSEL_table[(retval >> SYS_MIN_OFFSET) & SYS_MIN_MASK];
}

static int bq24296_set_vsys_voltage(struct regulator_dev *dev, int min_uV, int max_uV,
			       unsigned *selector)
{
	struct bq24296_device_info *di = rdev_get_drvdata(dev);
	int idx = dev->desc->id;

	printk("bq24296_set_vsys_voltage(%d, %d, %d, %u)\n", idx, min_uV, max_uV, *selector);
// The driver should select the voltage closest to min_uV by scanning vsys_VSEL_table

// disabled/untested
	return 0;

	/* set system voltage */

	return bq24296_update_reg(di->client,
				  POWER_ON_CONFIGURATION_REGISTER,
				  *selector,	/* 3.0V + 0.2V */
				  SYS_MIN_MASK << SYS_MIN_OFFSET);
}

static int bq24296_get_otg_voltage(struct regulator_dev *dev)
{
	struct bq24296_device_info *di = rdev_get_drvdata(dev);
	int idx = dev->desc->id;
	int ret;
	u8 retval;

	printk("bq24296_get_otg_voltage(%d)\n", idx);

	ret = bq24296_read(di->client, THERMAL_REGULATION_CONTROL_REGISTER, &retval, 1);
	if (ret < 0)
		return ret;
	printk(" => %d uV\n", otg_VSEL_table[(retval >> BOOSTV_OFFSET) & BOOSTV_MASK]);
	return otg_VSEL_table[(retval >> BOOSTV_OFFSET) & BOOSTV_MASK];
}

static int bq24296_set_otg_voltage(struct regulator_dev *dev, int min_uV, int max_uV,
			       unsigned *selector)
{
	struct bq24296_device_info *di = rdev_get_drvdata(dev);
	int idx = dev->desc->id;

	printk("bq24296_set_otg_voltage(%d, %d, %d, %u)\n", idx, min_uV, max_uV, *selector);
// The driver should select the voltage closest to min_uV by scanning otg_VSEL_table

// disabled/untested
	return 0;

	/* set OTG step up converter voltage */

	return bq24296_update_reg(di->client,
				  THERMAL_REGULATION_CONTROL_REGISTER,
				  *selector,
				  BOOSTV_MASK << BOOSTV_OFFSET);
}

static int bq24296_get_otg_current_limit(struct regulator_dev *dev)
{
	struct bq24296_device_info *di = rdev_get_drvdata(dev);
	int idx = dev->desc->id;
	int ret;
	u8 retval;

	printk("bq24296_get_otg_current_limit(%d)\n", idx);

	ret = bq24296_read(di->client, POWER_ON_CONFIGURATION_REGISTER, &retval, 1);
	if (ret < 0)
		return ret;

	return ((retval >> OTG_MODE_CURRENT_CONFIG_OFFSET) & OTG_MODE_CURRENT_CONFIG_MASK) ? 1000000 : 1500000;	/* 1.0A or 1.5A */
}

static int bq24296_set_otg_current_limit(struct regulator_dev *dev,
				     int min_uA, int max_uA)
{
	struct bq24296_device_info *di = rdev_get_drvdata(dev);
	int idx = dev->desc->id;

	printk("bq24296_set_otg_current_limit(%d, %d, %d)\n", idx, min_uA, max_uA);

	/* set OTG current limit in bit 0 of POWER_ON_CONFIGURATION_REGISTER */

	if(max_uA < 1250000)
		return bq24296_update_reg(di->client,POWER_ON_CONFIGURATION_REGISTER, OTG_MODE_CURRENT_CONFIG_500MA << OTG_MODE_CURRENT_CONFIG_OFFSET,OTG_MODE_CURRENT_CONFIG_MASK << OTG_MODE_CURRENT_CONFIG_OFFSET);	// choose 1A
	else
		return bq24296_update_reg(di->client,POWER_ON_CONFIGURATION_REGISTER, OTG_MODE_CURRENT_CONFIG_1300MA << OTG_MODE_CURRENT_CONFIG_OFFSET,OTG_MODE_CURRENT_CONFIG_MASK << OTG_MODE_CURRENT_CONFIG_OFFSET);	// choose 1.5A
}

static int bq24296_otg_enable(struct regulator_dev *dev)
{ /* enable OTG step up converter */
	struct bq24296_device_info *di = rdev_get_drvdata(dev);
	int idx = dev->desc->id;

	printk("%s(%d)\n", __func__, idx);

	/* check if battery is present and reject if no battery */
	if (!bq24296_battery_present(di)) {
		dev_warn(&di->client->dev, "can enable otg only with installed battery and no overtemperature\n");
		return -EBUSY;
	}

	bq24296_update_en_hiz_disable(di);
	mdelay(5);
	return bq24296_update_charge_mode(di, CHARGE_MODE_CONFIG_OTG_OUTPUT);
	// could check/wait with timeout that r8 indicates OTG mode
}

static int bq24296_otg_disable(struct regulator_dev *dev)
{ /* disable OTG step up converter */
	struct bq24296_device_info *di = rdev_get_drvdata(dev);
	int idx = dev->desc->id;

	printk("%s(%d)\n", __func__, idx);

	return bq24296_update_charge_mode(di, CHARGE_MODE_CONFIG_CHARGE_DISABLE);
	// could check/wait with timeout that r8 indicates non-OTG mode
}

static int bq24296_otg_is_enabled(struct regulator_dev *dev)
{ /* check if OTG converter is enabled */
	struct bq24296_device_info *di = rdev_get_drvdata(dev);
	int idx = dev->desc->id;
	int ret;
	u8 retval;

	printk("%s(%d)\n", __func__, idx);

	ret = bq24296_read(di->client, POWER_ON_CONFIGURATION_REGISTER, &retval, 1);
	if (ret < 0)
		return 0;	/* assume disabled */

	// we could also check r8 for OTG mode
	// return ((di->r8 >> VBUS_OFFSET) && VBUS_MASK) == VBUS_OTG;
	// which one is better?

	/* is bit 5 of POWER_ON_CONFIGURATION_REGISTER set? */
	return ((retval >> CHARGE_MODE_CONFIG_OFFSET) & CHARGE_MODE_CONFIG_MASK) == CHARGE_MODE_CONFIG_OTG_OUTPUT;
}

static struct regulator_ops vsys_ops = {
	.get_voltage = bq24296_get_vsys_voltage,
	.set_voltage = bq24296_set_vsys_voltage,	/* change vsys voltage */
};

static struct regulator_ops otg_ops = {
	.get_voltage = bq24296_get_otg_voltage,
	.set_voltage = bq24296_set_otg_voltage,	/* change OTG voltage */
	.get_current_limit = bq24296_get_otg_current_limit,	/* get OTG current limit */
	.set_current_limit = bq24296_set_otg_current_limit,	/* set OTG current limit */
	.enable = bq24296_otg_enable,	/* turn on OTG mode */
	.disable = bq24296_otg_disable,	/* turn off OTG mode */
	.is_enabled = bq24296_otg_is_enabled,
};

static struct of_regulator_match bq24296_regulator_matches[] = {
	[VSYS_REGULATOR] = { .name = "bq2429x-vsys"},
	[OTG_REGULATOR] ={  .name = "bq2429x-otg"},
};

/* device tree support */

#ifdef CONFIG_OF
static struct bq24296_board *bq24296_parse_dt(struct bq24296_device_info *di)
{
	struct bq24296_board *pdata;
	struct device_node *np;
	struct device_node *regulators;
	struct of_regulator_match *matches;
	static struct regulator_init_data *reg_data;
	int idx = 0, count, ret;
	u32 val;

	DBG("%s,line=%d\n", __func__,__LINE__);

	np = of_node_get(di->dev->of_node);
	if (!np) {
		dev_err(&di->client->dev, "could not find bq2429x DT node\n");
		return NULL;
	}
	pdata = devm_kzalloc(di->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return NULL;
	if (of_property_read_u32_array(np, "ti,chg_current", pdata->chg_current, 3)) {
		dev_err(&di->client->dev, "charge current not specified\n");
		return NULL;
	}

	// dc_det_pin - if 0, charger is switched by driver to 2048mA, otherwise 512mA
	pdata->dc_det_pin = devm_gpiod_get_index(&di->client->dev, "dc-det", 0, GPIOD_IN);

// FIXME: check for bq24296 (297 has no det gpio)

	if (IS_ERR(pdata->dc_det_pin)) {
		if (PTR_ERR(pdata->dc_det_pin) == -EPROBE_DEFER)
			return NULL;
		dev_err(&di->client->dev, "invalid det gpio: %ld\n", PTR_ERR(pdata->dc_det_pin));
		pdata->dc_det_pin = NULL;
	}

	of_node_get(np);
	regulators = of_get_child_by_name(np, "regulators");
	if (!regulators) {
		dev_err(&di->client->dev, "regulator node not found\n");
		return NULL;
	}

	count = ARRAY_SIZE(bq24296_regulator_matches);
	matches = bq24296_regulator_matches;

	ret = of_regulator_match(&di->client->dev, regulators, matches, count);
// printk("%d matches\n", ret);
	of_node_put(regulators);
	if (ret < 0) {
		dev_err(&di->client->dev, "Error parsing regulator init data: %d\n",
			ret);
		return NULL;
	}

	if (ret != count) {
		dev_err(&di->client->dev, "Found %d of expected %d regulators\n",
			ret, count);
		return NULL;
	}

	regulators = of_get_next_child(regulators, NULL);	// get first regulator (vsys)
	if (!of_property_read_u32(regulators, "regulator-max-microvolt", &val)) {
		dev_err(&di->client->dev, "found regulator-max-microvolt = %u\n", val);
		max_VSYS_uV = val;	// limited by device tree
	}

	reg_data = devm_kzalloc(&di->client->dev, (sizeof(struct regulator_init_data)
					* NUM_REGULATORS), GFP_KERNEL);
	if (!reg_data)
		return NULL;

	di->pmic_init_data = reg_data;

	for (idx = 0; idx < ret; idx++) {
// printk("matches[%d].of_node = %p\n", idx, matches[idx].of_node);
		if (!matches[idx].init_data || !matches[idx].of_node)
			continue;

		memcpy(&reg_data[idx], matches[idx].init_data,
				sizeof(struct regulator_init_data));

	}

	return pdata;
}

#else
static struct bq24296_board *bq24296_parse_dt(struct bq24296_device_info *di)
{
	return NULL;
}
#endif

#ifdef CONFIG_OF
static struct of_device_id bq24296_charger_of_match[] = {
	{ .compatible = "ti,bq24296", .data = (void *) 0 },
	{ .compatible = "ti,bq24297", .data = (void *) 1 },
	{ .compatible = "mps,mp2624", .data = (void *) 2 },	// can control VSYS-VBATT level but not OTG max power
	{ },
};
MODULE_DEVICE_TABLE(of, bq24296_charger_of_match);
#endif

static int bq24296_charger_suspend(struct device *dev, pm_message_t state)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq24296_device_info *di = i2c_get_clientdata(client);

	// turn off otg?
	cancel_delayed_work_sync(&di->usb_detect_work);
	return 0;
}

static int bq24296_charger_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq24296_device_info *di = i2c_get_clientdata(client);

	schedule_delayed_work(&di->usb_detect_work, msecs_to_jiffies(50));
	return 0;
}

static void bq24296_charger_shutdown(struct i2c_client *client)
{ /* make sure we turn off OTG mode on power down */
	struct bq24296_device_info *di = i2c_get_clientdata(client);

	if (bq24296_otg_is_enabled(di->rdev[1]))
		bq24296_otg_disable(di->rdev[1]);	/* turn off otg regulator */
}

/* SYSFS interface */

/*
 * sysfs max_current store
 * set the max current drawn from USB
 */

static ssize_t
bq24296_input_current_limit_uA_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t n)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq24296_device_info *di = i2c_get_clientdata(client);
	int cur = 0;
	int status = 0;

	status = kstrtoint(buf, 10, &cur);
	if (status)
		return status;
	if (cur < 0)
		return -EINVAL;

	printk("bq24296_input_current_limit_uA_store: set input max current to %u uA -> %02x\n", cur, bq24296_limit_current_mA_to_bits(cur/1000));
	if (cur < 80000)
		status = bq24296_update_input_current_limit(di, -1);	/* High-Z */
	else
		status = bq24296_update_input_current_limit(di, bq24296_limit_current_mA_to_bits(cur/1000));

	return (status == 0) ? n : status;
}

/*
 * sysfs max_current show
 * reports current drawn from VBUS
 * note: actual input current limit is the lower of I2C register and ILIM resistor
 */

static ssize_t bq24296_input_current_limit_uA_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq24296_device_info *di = i2c_get_clientdata(client);
	int cur = bq24296_input_current_limit_uA(di);

	if (cur < 0)
		return cur;

	return scnprintf(buf, PAGE_SIZE, "%u\n", cur);
}

// checkme: how does this relate to the OTG regulator getters/setters?

/*
 * sysfs otg store
 */
static ssize_t
bq24296_otg_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t n)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq24296_device_info *di = i2c_get_clientdata(client);
	int cur = 0;
	int status = 0;

	status = kstrtoint(buf, 10, &cur);
	if (status)
		return status;
	if (cur < 0)
		return -EINVAL;
	printk("bq24296_otg_store: set OTG max current %u uA\n", cur);
	bq24296_update_en_hiz_disable(di);
	mdelay(5);
	if(cur < 500000)
		status = bq24296_update_reg(di->client,POWER_ON_CONFIGURATION_REGISTER,0 << 5,0x01 << 5);	// disable OTG
	else if(cur < 1250000)
		status = bq24296_update_reg(di->client,POWER_ON_CONFIGURATION_REGISTER,((1 << 5)|OTG_MODE_CURRENT_CONFIG_500MA),((0x01 << 5)|0x01));	// enable 1A
	else
		status = bq24296_update_reg(di->client,POWER_ON_CONFIGURATION_REGISTER,((1 << 5)|OTG_MODE_CURRENT_CONFIG_1300MA),((0x01 << 5)|0x01));	// enable 1.5A
#if 1
	{
	u8 retval = 0xff;
	bq24296_read(di->client, POWER_ON_CONFIGURATION_REGISTER, &retval, 1);
	printk("bq24296_otg_store: POWER_ON_CONFIGURATION_REGISTER = %02x\n", retval);
	}
#endif
	return (status < 0) ? status : n;
}

/*
 * sysfs otg show
 */
static ssize_t bq24296_otg_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq24296_device_info *di = i2c_get_clientdata(client);
	int ret;
	u8 retval = 0;
	int cur = 0;

	ret = bq24296_read(di->client, POWER_ON_CONFIGURATION_REGISTER, &retval, 1);
	if (ret < 0) {
		dev_err(&di->client->dev, "%s: err %d\n", __func__, ret);
	}
	// check status register if it is really on: REG08[7:6] is set to 11
	if(retval & 0x20)	// OTG CONFIG
		{
		if(retval & 0x01)
			cur=1500000;
		else
			cur=1000000;
		}
	return scnprintf(buf, PAGE_SIZE, "%u\n", cur);
}

// can be removed if handled as property
static DEVICE_ATTR(max_current, 0644, bq24296_input_current_limit_uA_show,
			bq24296_input_current_limit_uA_store);

// do we need that? Only if there is no mechanism to set the regulator from user-space
static DEVICE_ATTR(otg, 0644, bq24296_otg_show, bq24296_otg_store);

/* power_supply interface */

static int bq24296_get_property(struct power_supply *psy,
				    enum power_supply_property psp,
				    union power_supply_propval *val)
{
	struct bq24296_device_info *di = power_supply_get_drvdata(psy);
	int ret;
	u8 retval = 0;

	DBG("%s,line=%d prop=%d\n", __func__,__LINE__, psp);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		switch((di->r8 >> CHRG_OFFSET) & CHRG_MASK) {
			case CHRG_NO_CHARGING:	val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING; break;
			case CHRG_PRE_CHARGE:	val->intval = POWER_SUPPLY_STATUS_CHARGING; break;
			case CHRG_FAST_CHARGE:	val->intval = POWER_SUPPLY_STATUS_CHARGING; break;
			case CHRG_CHRGE_DONE:	val->intval = POWER_SUPPLY_STATUS_FULL; break;
		}
		break;

	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		switch((di->r8 >> CHRG_OFFSET) & CHRG_MASK) {
			case CHRG_NO_CHARGING:	val->intval = POWER_SUPPLY_CHARGE_TYPE_NONE; break;
			case CHRG_PRE_CHARGE:	val->intval = POWER_SUPPLY_CHARGE_TYPE_TRICKLE; break;
			case CHRG_FAST_CHARGE:	val->intval = POWER_SUPPLY_CHARGE_TYPE_FAST; break;
			case CHRG_CHRGE_DONE:	val->intval = POWER_SUPPLY_CHARGE_TYPE_FAST; break;
		}
		break;

#if FIXME
	case POWER_SUPPLY_PROP_HEALTH:
		switch((di->r9 >> CHRG_FAULT_OFFSET) & CHRG_FAULT_MASK) {
			case 0:	val->intval = POWER_SUPPLY_HEALTH_GOOD; break;
			case 1:	val->intval = POWER_SUPPLY_HEALTH_OVERHEAT; break;
			case 2:	val->intval = POWER_SUPPLY_HEALTH_SAFETY_TIMER_EXPIRE; break;
			case 3:	val->intval = POWER_SUPPLY_HEALTH_WATCHDOG_TIMER_EXPIRE; break;
		}
		break;
#endif
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		if(bq24296_input_present(di)) {
			if ((di->r8 & DPM_STAT) != 0)
				val->intval = bq24296_get_vindpm_uV(di);
			else
				val->intval = 5000000;	/* power good: assume VBUS 5V */
		}
		else
			val->intval = 0;	/* power not good: assume 0V */
		break;

	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		val->intval = bq24296_input_current_limit_uA(di);
//		printk("bq24296 CURRENT_MAX: %u mA\n", val->intval);
		break;

	case POWER_SUPPLY_PROP_CURRENT_NOW:
		switch((di->r8 >> CHRG_OFFSET) & CHRG_MASK) {
			case CHRG_NO_CHARGING:
			case CHRG_CHRGE_DONE:
				val->intval = 0;	// assume not charging current
//				printk("bq24296 CURRENT_NOW: %u mA\n", val->intval = ret);
				break;

			case CHRG_PRE_CHARGE:
				ret = bq24296_read(di->client, PRE_CHARGE_TERMINATION_CURRENT_CONTROL_REGISTER, &retval, 1);
//				printk("bq24296: PRE_CHARGE_TERMINATION_CURRENT_CONTROL_REGISTER %02x\n", retval);
				if (ret < 0) {
					dev_err(&di->client->dev, "%s: err %d\n", __func__, ret);
				}
				ret = 128000 * ((retval >> PRE_CHARGE_CURRENT_LIMIT_OFFSET) & PRE_CHARGE_CURRENT_LIMIT_MASK) + 128000;	// return precharge limit
				val->intval = bq24296_input_current_limit_uA(di);
				/* return lower of both */
				if (ret < val->intval)
					val->intval = ret;
//				printk("bq24296 CURRENT_NOW: %u mA\n", val->intval);
				break;

			case CHRG_FAST_CHARGE:
				ret = bq24296_read(di->client, CHARGE_CURRENT_CONTROL_REGISTER, &retval, 1);
//				printk("bq24296: FAST_CHARGE CHARGE_CURRENT_CONTROL_REGISTER %02x\n", retval);
				if (ret < 0) {
					dev_err(&di->client->dev, "%s: err %d\n", __func__, ret);
				}
				ret = 64000 * ((retval >> CHARGE_CURRENT_OFFSET) & CHARGE_CURRENT_MASK) + 512000;
				val->intval = bq24296_input_current_limit_uA(di);
				/* return lower of both */
				if (ret < val->intval)
					val->intval = ret;
//				printk("bq24296 CURRENT_NOW: %u mA\n", val->intval);
				break;
		}
		break;

	case POWER_SUPPLY_PROP_TEMP:
		// FIXME: deduce values from BHOT and BCOLD settings if boost mode is active
		// otherwise we report the defaults from the chip spec
		if (di->r9 & 0x02)
			val->intval = -100;	// too cold (-10C)
		else if (di->r9 & 0x01)
			val->intval = 600;	// too hot (60C)
		else
			val->intval = 225;	// ok (22.5C)
		break;

	case POWER_SUPPLY_PROP_ONLINE:	/* charger online, i.e. VBUS */
		val->intval = bq24296_input_present(di);	/* power is good */
		break;

	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = bq24296_battery_present(di);
		break;

#if 0	/* this would indicate a low battery! */
	case POWER_SUPPLY_PROP_???:
		val->intval = !(r8 & VSYS_STAT);	// VBAT > VSYSMIN
		val->intval = bq24296_battery_present(di);
		break;
#endif

	default:
		return -EINVAL;
	}

	return 0;
}

static int bq24296_set_property(struct power_supply *psy,
				enum power_supply_property psp,
				const union power_supply_propval *val)
{
	struct bq24296_device_info *di = power_supply_get_drvdata(psy);

	DBG("%s,line=%d prop=%d\n", __func__,__LINE__, psp);

	switch (psp) {
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		if (val->intval < 80000)
			return bq24296_update_input_current_limit(di, -1);	/* High-Z mode */
		return bq24296_update_input_current_limit(di, bq24296_limit_current_mA_to_bits(val->intval/1000));
	default:
		return -EPERM;
	}

	return 0;
}

static int bq24296_writeable_property(struct power_supply *psy,
					enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		return 1;
	default:
		break;
	}

	return 0;
}

static enum power_supply_property bq24296_charger_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_PRESENT,
};

static const struct power_supply_desc bq24296_power_supply_desc[] = {
	{
	.name			= "bq24296",
	.type			= POWER_SUPPLY_TYPE_USB,
	.properties		= bq24296_charger_props,
	.num_properties		= ARRAY_SIZE(bq24296_charger_props),
	.get_property		= bq24296_get_property,
	.set_property		= bq24296_set_property,
	.property_is_writeable	= bq24296_writeable_property,
	},
	{
	.name			= "bq24297",
	.type			= POWER_SUPPLY_TYPE_USB,
	.properties		= bq24296_charger_props,
	.num_properties		= ARRAY_SIZE(bq24296_charger_props),
	.get_property		= bq24296_get_property,
	.set_property		= bq24296_set_property,
	.property_is_writeable	= bq24296_writeable_property,
	},
	{
	.name			= "mp2624",
	.type			= POWER_SUPPLY_TYPE_USB,
	.properties		= bq24296_charger_props,
	.num_properties		= ARRAY_SIZE(bq24296_charger_props),
	.get_property		= bq24296_get_property,
	.set_property		= bq24296_set_property,
	.property_is_writeable	= bq24296_writeable_property,
	},
};

/* PROBE */

static int bq24296_charger_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct bq24296_device_info *di;
	u8 retval = 0;
	struct bq24296_board *pdev;
	struct device_node *bq24296_node;
	struct power_supply_config psy_cfg = { };
	struct regulator_config config = { };
	struct regulator_init_data *init_data;
	struct regulator_dev *rdev;
	int i;
	int ret = -EINVAL;

	DBG("%s,line=%d\n", __func__,__LINE__);

	bq24296_node = of_node_get(client->dev.of_node);
	if (!bq24296_node) {
		dev_warn(&client->dev, "could not find bq24296 DT node\n");
	}

	di = devm_kzalloc(&client->dev, sizeof(*di), GFP_KERNEL);

#if 0
	printk("%s: di = %px\n", __func__, di);
#endif

	if (di == NULL) {
		dev_err(&client->dev, "failed to allocate device info data\n");
		ret = -ENOMEM;
		goto fail_probe;
	}

	di->dev = &client->dev;
	i2c_set_clientdata(client, di);
	di->client = client;

	if (bq24296_node)
		pdev = bq24296_parse_dt(di);
	else
		pdev = dev_get_platdata(di->dev);

	if (!pdev) {
		dev_err(&client->dev, "failed to get platform data\n");
		ret = -EPROBE_DEFER;
		goto fail_probe;
	}

	DBG("%s,line=%d chg_current =%d usb_input_current = %d adp_input_current =%d \n", __func__,__LINE__,
		pdev->chg_current[0],pdev->chg_current[1],pdev->chg_current[2]);

	/******************get set current******/
	if (pdev->chg_current[0] && pdev->chg_current[1] && pdev->chg_current[2]){
		di->chg_current = bq24296_chg_current_mA_to_bits(pdev->chg_current[0] );
		di->usb_input_current  = bq24296_limit_current_mA_to_bits(pdev->chg_current[1]);
		di->adp_input_current  = bq24296_limit_current_mA_to_bits(pdev->chg_current[2]);
	}
	else {
		di->chg_current = bq24296_chg_current_mA_to_bits(1000);
		di->usb_input_current  = bq24296_limit_current_mA_to_bits(500);
		di->adp_input_current  = bq24296_limit_current_mA_to_bits(2000);
	}

	DBG("%s,line=%d chg_current =%d usb_input_current = %d adp_input_current =%d \n", __func__,__LINE__,
		di->chg_current,di->usb_input_current,di->adp_input_current);

	/****************************************/
	/* get the vendor id */
	ret = bq24296_read(di->client, VENDOR_STATS_REGISTER, &retval, 1);
	if (ret < 0) {
		dev_err(&di->client->dev, "%s(): Failed in reading register "
				"0x%02x\n", __func__, VENDOR_STATS_REGISTER);
		ret = -EPROBE_DEFER;	// tray again later
		goto fail_probe;
	}

	if ((retval & 0xa7) != 0x20) {
		dev_err(&client->dev, "not a bq24296/97: %02x\n", retval);
		ret = -ENODEV;
		goto fail_probe;
	}

	di->id = id;
	di->dc_det_pin = pdev->dc_det_pin;
	di->psel_pin = pdev->psel_pin;

/* we can also read and save the IINLIM value inherited from the boot process here! */

	init_data = di->pmic_init_data;
	if (!init_data)
		return -EINVAL;

	mutex_init(&di->var_lock);
	di->workqueue = create_singlethread_workqueue("bq24296_irq");
	INIT_WORK(&di->irq_work, bq2729x_irq_work_func);
	INIT_DELAYED_WORK(&di->usb_detect_work, usb_detect_work_func);

	// di->usb_nb.notifier_call = bq24296_bci_usb_ncb;

	psy_cfg.drv_data = di;
	di->usb = devm_power_supply_register(&client->dev,
						&bq24296_power_supply_desc[id->driver_data],
						&psy_cfg);
	if (IS_ERR(di->usb)) {
		ret = PTR_ERR(di->usb);
		dev_err(&client->dev, "failed to register as USB power_supply: %d\n", ret);
		goto fail_probe;
	}

	for (i = 0; i < NUM_REGULATORS; i++, init_data++) {
		/* Register the regulators */

		di->desc[i].id = i;
		di->desc[i].name = bq24296_regulator_matches[i].name;
// printk("%d: %s %s\n", i, di->desc[i].name, di->desc[i].of_match);
		di->desc[i].type = REGULATOR_VOLTAGE;
		di->desc[i].owner = THIS_MODULE;

		switch(i) {
			case VSYS_REGULATOR:
				di->desc[i].ops = &vsys_ops;
				di->desc[i].n_voltages = ARRAY_SIZE(vsys_VSEL_table);
				di->desc[i].volt_table = vsys_VSEL_table;
				break;
			case OTG_REGULATOR:
				di->desc[i].ops = &otg_ops;
				di->desc[i].n_voltages = ARRAY_SIZE(otg_VSEL_table);
				di->desc[i].volt_table = otg_VSEL_table;
				break;
		}

		config.dev = di->dev;
		config.init_data = init_data;
		config.driver_data = di;
		config.of_node = bq24296_regulator_matches[i].of_node;
// printk("%d: %s\n", i, config.of_node?config.of_node->name:"?");

		rdev = devm_regulator_register(&client->dev, &di->desc[i],
					       &config);
		if (IS_ERR(rdev)) {
			dev_err(di->dev,
				"failed to register %s regulator %d %s\n",
				client->name, i, di->desc[i].name);
			return PTR_ERR(rdev);
		}

		/* Save regulator for cleanup */
		di->rdev[i] = rdev;
	}

	ret = bq24296_init_registers(di);
	if (ret < 0) {
		dev_err(&client->dev, "failed to initialize registers: %d\n", ret);
		goto fail_probe;
	}

#if 1
	di->prev_r8 = 0xff;
	di->prev_r9 = 0xff;
#endif
	ret = devm_request_threaded_irq(&client->dev, client->irq,
				NULL, bq2729x_chg_irq_func,
				IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				client->name,
				di);
	if (ret < 0) {
		dev_warn(&client->dev, "failed to request chg_irq: %d\n", ret);
		// run with polling
//		goto fail_probe;
	}

	if (device_create_file(&client->dev, &dev_attr_max_current))
		dev_warn(&client->dev, "could not create sysfs file max_current\n");

	if (device_create_file(&client->dev, &dev_attr_otg))
		dev_warn(&client->dev, "could not create sysfs file otg\n");

	if (device_create_file(&client->dev, &dev_attr_registers))
		dev_warn(&client->dev, "could not create sysfs file registers\n");

	schedule_delayed_work(&di->usb_detect_work, 0);

	DBG("%s ok", __func__);

	return 0;

fail_probe:
	DBG("%s failed %d", __func__, ret);
	return ret;
}

static int bq24296_charger_remove(struct i2c_client *client)
{
	struct bq24296_device_info *di = i2c_get_clientdata(client);

	device_remove_file(di->dev, &dev_attr_max_current);
	device_remove_file(di->dev, &dev_attr_otg);
	device_remove_file(di->dev, &dev_attr_registers);

	return 0;
}

static const struct i2c_device_id bq24296_charger_id[] = {
	{ "bq24296", 0 },
	{ "bq24297", 1 },
	{ "mp2624", 2 },
	{ },
};

MODULE_DEVICE_TABLE(i2c, bq24296_charger_id);

static struct i2c_driver bq24296_charger_driver = {
	.probe = bq24296_charger_probe,
	.remove = bq24296_charger_remove,
	.shutdown = bq24296_charger_shutdown,
	.id_table = bq24296_charger_id,
	.driver = {
		.name = "bq2429x_charger",
	//	.pm = &bq2429x_pm_ops,
		.of_match_table =of_match_ptr(bq24296_charger_of_match),
		.suspend = bq24296_charger_suspend,
		.resume = bq24296_charger_resume,
	},
};

module_i2c_driver(bq24296_charger_driver);

MODULE_AUTHOR("Rockchip");
MODULE_DESCRIPTION("TI BQ24296/7 charger driver");
MODULE_LICENSE("GPL");
