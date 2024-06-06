/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include <zephyr/drivers/gpio.h>

#include "battery.h"
#include "battery_smart.h"
#include "battery_fuel_gauge.h"
#include "board_charger.h"
#include "charge_manager.h"
#include "charge_state.h"
#include "charger.h"
#include "console.h"
#include "cypress_pd_common.h"
#include "driver/charger/isl9241.h"
#include "extpower.h"
#include "hooks.h"
#include "i2c.h"
#include "power_sequence.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_CHARGER, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_CHARGER, format, ## args)

static bool charger_psys_enable_flag;

#ifdef CONFIG_PLATFORM_EC_CHARGER_INIT_CUSTOM
static void charger_chips_init(void);
static void charger_chips_init_retry(void)
{
	charger_chips_init();
}
DECLARE_DEFERRED(charger_chips_init_retry);

static void charger_chips_init(void)
{
	/* Battery present need ADC function ready, so change the initail priority
	 * after ADC
	 */

	const int no_battery_current_limit_override_ma = 3000;
	const struct battery_info *bi = battery_get_info();
	uint16_t val = 0x0000; /*default ac setting */
	uint32_t data = 0;
	int value;

	/*
	 * In our case the EC can boot before the charger has power so
	 * check if the charger is responsive before we try to init it
	 */

	if (i2c_read16(I2C_PORT_CHARGER, ISL9241_ADDR_FLAGS,
		ISL9241_REG_ACOK_REFERENCE, &data) != EC_SUCCESS) {
		CPRINTS("Retry Charger init");
		hook_call_deferred(&charger_chips_init_retry_data, 100*MSEC);
		return;
	}

	value = battery_is_charge_fet_disabled();

	/*
	 * Set control3 register to
	 * [14]: ACLIM Reload (1 Do not reload)
	 * [8:9]: PSYS Gain (11 Default)
	 */
	if (i2c_write16(I2C_PORT_CHARGER, ISL9241_ADDR_FLAGS,
		ISL9241_REG_CONTROL3, ISL9241_CONTROL3_ACLIM_RELOAD |
		ISL9241_CONTROL3_PSYS_GAIN))
		goto init_fail;

	/* reverse the flag if no error */
	if (value != -1)
		value = !value;
	/*
	 * When there is no battery, override charger current limit to
	 * prevent brownout during boot.
	 */
	if (value == -1) {
		ccprints("No Battery Found - Override Current Limit to %dmA",
			 no_battery_current_limit_override_ma);
		charger_set_input_current_limit(
			CHARGER_SOLO, no_battery_current_limit_override_ma);
	}

	/* According to Power team suggest, Set ACOK reference to 3.072V */
	if (i2c_write16(I2C_PORT_CHARGER, ISL9241_ADDR_FLAGS,
		ISL9241_REG_ACOK_REFERENCE, ISL9241_MV_TO_ACOK_REFERENCE(3072)))
		goto init_fail;


	/*
	 * Set the MaxSystemVoltage to battery maximum,
	 * 0x00=disables switching charger states
	 */
	if (value == -1) {
		if (i2c_write16(I2C_PORT_CHARGER, ISL9241_ADDR_FLAGS,
			ISL9241_REG_MAX_SYSTEM_VOLTAGE, 15400))
			goto init_fail;
	} else {
		if (i2c_write16(I2C_PORT_CHARGER, ISL9241_ADDR_FLAGS,
			ISL9241_REG_MAX_SYSTEM_VOLTAGE, bi->voltage_max))
			goto init_fail;
	}

	/*
	 * Set the MinSystemVoltage to battery minimum,
	 * 0x00=disables all battery charging
	 */
	if (i2c_write16(I2C_PORT_CHARGER, ISL9241_ADDR_FLAGS,
		ISL9241_REG_MIN_SYSTEM_VOLTAGE, bi->voltage_min))
		goto init_fail;

	/*
	 * Set control2 register to
	 * [15:13]: Trickle Charging Current (011 128mA default)
	 * [10:9]: Prochot# Debounce time (01 100μs)
	 * [3]: General Purpose Comparator (1 Disable)
	 */
	if (i2c_write16(I2C_PORT_CHARGER, ISL9241_ADDR_FLAGS,
		ISL9241_REG_CONTROL2,
		ISL9241_CONTROL2_TRICKLE_CHG_CURR(128) |
		ISL9241_CONTROL2_PROCHOT_DEBOUNCE_100 |
		ISL9241_CONTROL2_GENERAL_PURPOSE_COMPARATOR))
		goto init_fail;

	if (i2c_write16(I2C_PORT_CHARGER, ISL9241_ADDR_FLAGS,
		ISL9241_REG_CONTROL0, 0x0000))
		goto init_fail;

	val = ISL9241_CONTROL1_PROCHOT_REF_6800;
	val |= ((ISL9241_CONTROL1_SWITCHING_FREQ_724KHZ << 7) &
			ISL9241_CONTROL1_SWITCHING_FREQ_MASK);

	/* Make sure battery FET is enabled on EC on */
	val &= ~ISL9241_CONTROL1_BGATE_OFF;

	if (i2c_write16(I2C_PORT_CHARGER, ISL9241_ADDR_FLAGS,
		ISL9241_REG_CONTROL1, val))
		goto init_fail;

	/* TODO: should we need to talk to PD chip after initial complete ? */
	CPRINTS("ISL9241 customized initial complete!");
	return;

init_fail:
	CPRINTF("ISL9241 customer init failed!");
}
DECLARE_HOOK(HOOK_INIT, charger_chips_init, HOOK_PRIO_POST_I2C);
#endif

void charger_update(void)
{
	static int pre_ac_state;
	static int pre_dc_state;

	if (pre_ac_state != extpower_is_present() ||
		pre_dc_state != battery_is_present()) {
		CPRINTS("update charger!!");

		/*set dc prochot 7.680A/0x1E00(61w) 7.168A/0x1c00(55w)*/
		const struct battery_info *bi = battery_get_info();
		int rv = 0;

		if (bi->voltage_max == 17600) {
			rv |= i2c_write16(I2C_PORT_CHARGER, ISL9241_ADDR_FLAGS,
			ISL9241_REG_DC_PROCHOT, 0x1C00);
		} else {
			rv |= i2c_write16(I2C_PORT_CHARGER, ISL9241_ADDR_FLAGS,
			ISL9241_REG_DC_PROCHOT, 0x1E00);
		}
		if (rv)
			CPRINTS("Update DC prochot fail");

		pre_ac_state = extpower_is_present();
		pre_dc_state = battery_is_present();
	}
}
DECLARE_HOOK(HOOK_AC_CHANGE, charger_update, HOOK_PRIO_DEFAULT);
DECLARE_HOOK(HOOK_BATTERY_SOC_CHANGE, charger_update, HOOK_PRIO_DEFAULT);

void board_set_charge_limit(int port, int supplier, int charge_ma,
			    int max_ma, int charge_mv)
{
	int prochot_ma;

	if (charge_ma < CONFIG_PLATFORM_EC_CHARGER_DEFAULT_CURRENT_LIMIT) {
		charge_ma = CONFIG_PLATFORM_EC_CHARGER_DEFAULT_CURRENT_LIMIT;
	}
	/*
	 * ac prochot should bigger than input current
	 * And needs to be at least 128mA bigger than the adapter current
	 */
	prochot_ma = (DIV_ROUND_UP(charge_ma, 128) * 128);
	charge_ma = charge_ma * 95 / 100;

	if ((prochot_ma - charge_ma) < 128) {
		charge_ma = prochot_ma - 128;
	}

	charge_set_input_current_limit(charge_ma, charge_mv);
	/* sync-up ac prochot with current change */
	isl9241_set_ac_prochot(0, prochot_ma);
}

void charge_gate_onoff(uint8_t enable)
{
	int control0 = 0x0000;
	int control1 = 0x0000;

	if (i2c_read16(I2C_PORT_CHARGER, ISL9241_ADDR_FLAGS,
		ISL9241_REG_CONTROL0, &control0)) {
		CPRINTS("read gate control1 fail");
	}

	if (i2c_read16(I2C_PORT_CHARGER, ISL9241_ADDR_FLAGS,
		ISL9241_REG_CONTROL1, &control1)) {
		CPRINTS("read gate control1 fail");
	}

	if (enable) {
		control0 &= ~ISL9241_CONTROL0_NGATE_OFF;
		control1 &= ~ISL9241_CONTROL1_BGATE_OFF;
		CPRINTS("B&N Gate off");
	} else {
		control0 |= ISL9241_CONTROL0_NGATE_OFF;
		control1 |= ISL9241_CONTROL1_BGATE_OFF;
		CPRINTS("B&N Gate on");
	}

	if (i2c_write16(I2C_PORT_CHARGER, ISL9241_ADDR_FLAGS,
		ISL9241_REG_CONTROL0, control0)) {
		CPRINTS("Update gate control0 fail");
	}

	if (i2c_write16(I2C_PORT_CHARGER, ISL9241_ADDR_FLAGS,
		ISL9241_REG_CONTROL1, control1)) {
		CPRINTS("Update gate control1 fail");
	}
}

void charger_psys_enable(uint8_t enable)
{
	int control1 = 0x0000;
	int control4 = 0x0000;
	int data = 0x0000;

	charger_psys_enable_flag = enable;

	if (i2c_read16(I2C_PORT_CHARGER, ISL9241_ADDR_FLAGS,
		ISL9241_REG_CONTROL1, &control1)) {
		CPRINTS("read psys control1 fail");
	}

	if (i2c_read16(I2C_PORT_CHARGER, ISL9241_ADDR_FLAGS,
		ISL9241_REG_CONTROL4, &control4)) {
		CPRINTS("read psys control1 fail");
	}

	if (enable) {
		control1 &= ~ISL9241_CONTROL1_IMON;
		control1 |= ISL9241_CONTROL1_PSYS;
		control4 &= ~ISL9241_CONTROL4_GP_COMPARATOR;
		/* Set ACOK reference Vadp>15V acok=4.544V, Vadp<=15V acok=3.072V */
		data = (cypd_get_active_port_voltage() > 15000 ? 0x0BC0 : 0x0800);
		CPRINTS("Power saving disable");
	} else {
		control1 |= ISL9241_CONTROL1_IMON;
		control1 &= ~ISL9241_CONTROL1_PSYS;
		control4 |= ISL9241_CONTROL4_GP_COMPARATOR;
		data = 0x0000;		/* Set ACOK reference to 0V */
		CPRINTS("Power saving enable");
	}


	if (i2c_write16(I2C_PORT_CHARGER, ISL9241_ADDR_FLAGS,
		ISL9241_REG_ACOK_REFERENCE, data)) {
		CPRINTS("Update ACOK reference fail");
	}

	if (i2c_write16(I2C_PORT_CHARGER, ISL9241_ADDR_FLAGS,
		ISL9241_REG_CONTROL1, control1)) {
		CPRINTS("Update psys control1 fail");
	}

	if (i2c_write16(I2C_PORT_CHARGER, ISL9241_ADDR_FLAGS,
		ISL9241_REG_CONTROL4, control4)) {
		CPRINTS("Update psys control4 fail");
	}
}

/* Called on AP S5 -> S3 transition */
static void board_charger_lpm_disable(void)
{
	charger_psys_enable(1);
}
DECLARE_HOOK(HOOK_CHIPSET_STARTUP, board_charger_lpm_disable, HOOK_PRIO_DEFAULT);
DECLARE_HOOK(HOOK_CHIPSET_RESUME, board_charger_lpm_disable, HOOK_PRIO_DEFAULT);

static void board_charger_lpm_enable(void)
{
	charger_psys_enable(0);
}
DECLARE_HOOK(HOOK_CHIPSET_SHUTDOWN, board_charger_lpm_enable, HOOK_PRIO_DEFAULT);
DECLARE_HOOK(HOOK_CHIPSET_SUSPEND, board_charger_lpm_enable, HOOK_PRIO_DEFAULT);
DECLARE_HOOK(HOOK_INIT, board_charger_lpm_enable, HOOK_PRIO_DEFAULT);

__override void board_hibernate(void)
{
	/* Turn off BGATE and NGATE for power saving */
	charger_psys_enable(0);
	charge_gate_onoff(0);
}

void acok_control(int voltage)
{
	static int pre_acok_data;
	int acok_data = 0x00;

	if (!charger_psys_enable_flag)
		return;

	if (voltage > 15000)
		acok_data = 0x0BC0; /*set ACOK 4.544V*/
	else
		acok_data = 0x0800; /*set ACOK 3.072V*/

	if (acok_data != pre_acok_data) {
		if (i2c_write16(I2C_PORT_CHARGER, ISL9241_ADDR_FLAGS,
				ISL9241_REG_ACOK_REFERENCE, acok_data)) {
			CPRINTS("Update ACOK reference fail");
		}
		pre_acok_data = acok_data;
	}
}
