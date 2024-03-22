/* Copyright 2021 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "adc_chip.h"
#include "common.h"
#include "hooks.h"
#include "temp_sensor.h"
#include "temp_sensor/thermistor.h"
#include "thermal.h"

/* ADC configuration */
const struct adc_t adc_channels[] = {
	[ADC_TEMP_SENSOR_1_CPU] = {
		.name = "TEMP_CPU",
		.input_ch = NPCX_ADC_CH0,
		.factor_mul = ADC_MAX_VOLT,
		.factor_div = ADC_READ_MAX + 1,
		.shift = 0,
	},
	[ADC_TEMP_SENSOR_2_CPU_VR] = {
		.name = "TEMP_CPU_VR",
		.input_ch = NPCX_ADC_CH1,
		.factor_mul = ADC_MAX_VOLT,
		.factor_div = ADC_READ_MAX + 1,
		.shift = 0,
	},
	[ADC_TEMP_SENSOR_3_WIFI] = {
		.name = "TEMP_WIFI",
		.input_ch = NPCX_ADC_CH6,
		.factor_mul = ADC_MAX_VOLT,
		.factor_div = ADC_READ_MAX + 1,
		.shift = 0,
	},
	[ADC_TEMP_SENSOR_4_DIMM] = {
		.name = "TEMP_DIMM",
		.input_ch = NPCX_ADC_CH7,
		.factor_mul = ADC_MAX_VOLT,
		.factor_div = ADC_READ_MAX + 1,
		.shift = 0,
	},
	[ADC_VBUS] = {  /* 5/39 voltage divider */
		.name = "VBUS",
		.input_ch = NPCX_ADC_CH2,
		.factor_mul = ADC_MAX_VOLT * 39,
		.factor_div = (ADC_READ_MAX + 1) * 5,
	},
	[ADC_PPVAR_IMON] = {  /* 872.3 mV/A */
		.name = "PPVAR_IMON",
		.input_ch = NPCX_ADC_CH3,
		.factor_mul = ADC_MAX_VOLT * 1433,
		.factor_div = (ADC_READ_MAX + 1) * 1250,
	},

};
BUILD_ASSERT(ARRAY_SIZE(adc_channels) == ADC_CH_COUNT);

/* Temperature sensor configuration */
const struct temp_sensor_t temp_sensors[] = {
	[TEMP_SENSOR_1_CPU] = { .name = "CPU",
				.type = TEMP_SENSOR_TYPE_BOARD,
				.read = get_temp_3v3_30k9_47k_4050b,
				.idx = ADC_TEMP_SENSOR_1_CPU },
	[TEMP_SENSOR_2_CPU_VR] = { .name = "CPU VR",
				   .type = TEMP_SENSOR_TYPE_BOARD,
				   .read = get_temp_3v3_30k9_47k_4050b,
				   .idx = ADC_TEMP_SENSOR_2_CPU_VR },
	[TEMP_SENSOR_3_WIFI] = { .name = "WIFI",
				 .type = TEMP_SENSOR_TYPE_BOARD,
				 .read = get_temp_3v3_30k9_47k_4050b,
				 .idx = ADC_TEMP_SENSOR_3_WIFI },
	[TEMP_SENSOR_4_DIMM] = { .name = "DIMM",
				 .type = TEMP_SENSOR_TYPE_BOARD,
				 .read = get_temp_3v3_30k9_47k_4050b,
				 .idx = ADC_TEMP_SENSOR_4_DIMM },
};
BUILD_ASSERT(ARRAY_SIZE(temp_sensors) == TEMP_SENSOR_COUNT);

/*
 * TODO(b/180681346): update for Alder Lake/brya
 *
 * Tiger Lake specifies 100 C as maximum TDP temperature.  THRMTRIP# occurs at
 * 130 C.  However, sensor is located next to DDR, so we need to use the lower
 * DDR temperature limit (85 C)
 */
static const struct ec_thermal_config thermal_cpu = {
	.temp_host = {
		[EC_TEMP_THRESH_HIGH] = C_TO_K(70),
		[EC_TEMP_THRESH_HALT] = C_TO_K(80),
	},
	.temp_host_release = {
		[EC_TEMP_THRESH_HIGH] = C_TO_K(65),
	},
	.temp_fan_off = C_TO_K(35),
	.temp_fan_max = C_TO_K(50),
};

/*
 * TODO(b/197478860): add the thermal sensor setting
 */
/* this should really be "const" */
struct ec_thermal_config thermal_params[] = {
	[TEMP_SENSOR_1_CPU] = thermal_cpu,
	[TEMP_SENSOR_2_CPU_VR] = thermal_cpu,
	[TEMP_SENSOR_3_WIFI] = thermal_cpu,
	[TEMP_SENSOR_4_DIMM] = thermal_cpu,
};
BUILD_ASSERT(ARRAY_SIZE(thermal_params) == TEMP_SENSOR_COUNT);
