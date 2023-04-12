
/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * ADC for checking BOARD ID.
 */

#include "adc.h"
#include "gpio/gpio_int.h"
#include "gpio.h"
#include "gpu.h"
#include "board_adc.h"
#include "console.h"
#include "system.h"
#include "hooks.h"
#include "i2c.h"
#include "flash_storage.h"

LOG_MODULE_REGISTER(gpu, LOG_LEVEL_INF);

#define VALID_BOARDID(ID1, ID0) ((ID1 << 8) + ID0)

static int module_present;
static int gpu_detected;
bool gpu_present(void)
{
	return module_present;
}


void check_gpu_module(void)
{
	int gpu_id_0 = get_hardware_id(ADC_GPU_BOARD_ID_0);
	int gpu_id_1 = get_hardware_id(ADC_GPU_BOARD_ID_1);

	gpio_enable_dt_interrupt(GPIO_INT_FROM_NODELABEL(int_chassis_open));

	switch (VALID_BOARDID(gpu_id_1, gpu_id_0)) {
	case VALID_BOARDID(BOARD_VERSION_12, BOARD_VERSION_12):
		LOG_INF("Detected dual interposer device");
		module_present = 1;
		break;
	case VALID_BOARDID(BOARD_VERSION_11, BOARD_VERSION_15):
	case VALID_BOARDID(BOARD_VERSION_13, BOARD_VERSION_15):
		LOG_INF("Detected single interposer device");
		module_present = 1;
		break;

	default:
		LOG_INF("No gpu module detected %d %d", gpu_id_0, gpu_id_1);
		/* Framework TODO remove for DVT, for now force on  unless feature is enabled */
		if (flash_storage_get(FLASH_FLAGS_ENABLE_GPU_DETECT) == 0) {
			module_present = 1;
		} else {
			module_present = 0;
		}
	break;
	}

	if (module_present) {
		gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_gpu_3v_5v_en), 1);
		gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_gpu_vsys_vadp_en), 1);
	} else {
		gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_gpu_3v_5v_en), 0);
		gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_edp_mux_pwm_sw), 0);
		gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_gpu_vsys_vadp_en), 0);
	}
}
DECLARE_DEFERRED(check_gpu_module);
DECLARE_HOOK(HOOK_INIT, check_gpu_module, HOOK_PRIO_INIT_ADC + 1);


void chassis_open_interrupt(enum gpio_signal signal)
{
	int open_state = gpio_pin_get_dt(GPIO_DT_FROM_NODELABEL(gpio_chassis_open_l));

	if (!open_state) {
		/* Make sure the module is off as fast as possible! */
		LOG_INF("Powering off GPU");
		gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_gpu_vsys_vadp_en), 0);
		gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_gpu_3v_5v_en), 0);
		gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_edp_mux_pwm_sw), 0);
		module_present = 0;
	} else {
		hook_call_deferred(&check_gpu_module_data, 50*MSEC);
	}
}

static void gpu_mux_configure(void)
{
	int rv = 1;
	int data;

	if (module_present) {
		/* TODO Setup real gpu detection, for now just detect thermal sensor*/
		/* Disable gpu detection until mux is fixed */
		rv = i2c_read8(I2C_PORT_GPU0, 0x4d, 0x00, &data);
		if (rv == EC_SUCCESS && flash_storage_get(FLASH_FLAGS_ENABLE_GPU_MUX)) {
			LOG_INF("dGPU detected, enabling mux");
			gpu_detected = 1;

			gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_edp_mux_pwm_sw), 1);
		} else {
			LOG_INF("dGPU not enabling mux");
		}
	}
}
DECLARE_HOOK(HOOK_CHIPSET_RESUME, gpu_mux_configure, HOOK_PRIO_DEFAULT);