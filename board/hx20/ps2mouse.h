/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * i2c to PS2 compat mouse emulation using hid-i2c to ps2 conversion
 */
#ifndef __CROS_EC_PS2MOUSE_H
#define __CROS_EC_PS2MOUSE_H


enum ps2_mouse_state {
	PS2MSTATE_RESET,
	PS2MSTATE_STREAM,
	PS2MSTATE_REMOTE,
	PS2MSTATE_WRAP,
	PS2MSTATE_CONSUME_1_BYTE,
	PS2MSTATE_CONSUME_1_BYTE_ACK,
};
/*TYPE_C_STATUS_DEVICE*/
enum ps2_mouse_command {
	PS2MOUSE_ID_PS2 = 0x00, /* 3 byte packet format */
	PS2MOUSE_ID_INTELLIMOUSE = 0x03, /*4 byte packet format*/
	PS2MOUSE_ID_INTELLIMOUSE_5BTN = 0x04, /*4 byte packet format*/

	PS2MOUSE_BAT_SUCCESS = 0xAA,
	PS2MOUSE_SET_SCALE_1 = 0xE6,
	PS2MOUSE_SET_SCALE_2 = 0xE7,
	PS2MOUSE_SET_RESOLUTION = 0xE8,
	PS2MOUSE_STATUS_REQUEST = 0xE9, /*respond with 3 byte packet */
	PS2MOUSE_SET_STREAM_MODE = 0xEA,
	PS2MOUSE_READ_DATA = 0xEB,
	PS2MOUSE_RESET_WRAP_MODE = 0xEC,
	PS2MOUSE_SET_WRAP_MODE = 0xEE,
	PS2MOUSE_SET_REMOTE_MODE = 0xF0,
	PS2MOUSE_GET_DEVICE_ID = 0xF2,
	PS2MOUSE_SET_SAMPLE_RATE = 0xF3,
	PS2MOUSE_ENABLE_DATA_REPORT = 0xF4,
	PS2MOUSE_DISABLE_DATA_REPORT = 0xF5,
	PS2MOUSE_SET_DEFAULTS = 0xF6,
	PS2MOUSE_ACKNOWLEDGE = 0xFA,
	PS2MOUSE_RESEND = 0xFE,
	PS2MOUSE_RESET = 0xFF,
};

enum ps2_mouse_task_evt {
	PS2MOUSE_EVT_INTERRUPT = BIT(0),
	PS2MOUSE_EVT_I2C_INTERRUPT = BIT(1),
	PS2MOUSE_EVT_POWERSTATE = BIT(2),
	PS2MOUSE_EVT_REENABLE = BIT(3),
	PS2MOUSE_EVT_AUX_DATA = BIT(4),
	PS2MOUSE_EVT_HC_DISABLE = BIT(5),
	PS2MOUSE_EVT_HC_ENABLE = BIT(6),


};

#define LEFT_BTN BIT(0)
#define RIGHT_BTN BIT(1)
#define MIDDLE_BTN BIT(2)
#define X_SIGN BIT(4)
#define Y_SIGN BIT(5)
#define X_OVERFLOW BIT(6)
#define Y_OVERFLOW BIT(7)

#define STATUS_MODE_REMOTE BIT(6)
#define STATUS_DATA_ENABLED BIT(5)

#define TOUCHPAD_I2C_HID_EP 0x2c
#define TOUCHPAD_I2C_CONTROL_EP 0x33

#define TOUCHPAD_I2C_RETRY_COUNT_TO_RENABLE 6

enum pixart_pct3854_regs {
	PCT3854_DESCRIPTOR	= 0x0020,
	PCT3854_REPORT_DESC	= 0x0021,
	PCT3854_COMMAND		= 0x0022,
	PCT3854_DATA		= 0x0023,
	PCT3854_INPUT		= 0x0024,
	PCT3854_OUTPUT		= 0x0025,
	PCT3854_VID			= 0x093A,
	PCT3854_PID			= 0x0255,
};
#define TOUCHPAD_I2C_HID_DESCRIPTOR 0x0020
void set_ps2_mouse_emulation(bool disable);
#endif	/* __CROS_EC_PS2MOUSE_H */
