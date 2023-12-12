/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef __CROS_EC_DRIVER_CEC_IT83XX_H
#define __CROS_EC_DRIVER_CEC_IT83XX_H

/* Consumer Electronics Control (CEC) */
#define IT83XX_CEC_BASE 0x00F02E00

#define IT83XX_CEC_CECDR REG8(IT83XX_CEC_BASE + 0x00)
#define IT83XX_CEC_CECFSTS REG8(IT83XX_CEC_BASE + 0x01)
#define IT83XX_CEC_CECFSTS_FCNT GENMASK(2, 0)
#define IT83XX_CEC_CECFSTS_FF BIT(4)
#define IT83XX_CEC_CECFSTS_FE BIT(5)
#define IT83XX_CEC_CECFSTS_FCLR BIT(6)
#define IT83XX_CEC_CECDLA REG8(IT83XX_CEC_BASE + 0x02)
#define IT83XX_CEC_CECDLA_DLA GENMASK(3, 0)
#define IT83XX_CEC_CECCTRL REG8(IT83XX_CEC_BASE + 0x03)
#define IT83XX_CEC_CECCTRL_ICC BIT(0)
#define IT83XX_CEC_CECCTRL_NBT BIT(3)
#define IT83XX_CEC_CECCTRL_EOM BIT(4)
#define IT83XX_CEC_CECCTRL_NKEN BIT(5)
#define IT83XX_CEC_CECSTS REG8(IT83XX_CEC_BASE + 0x04)
#define IT83XX_CEC_CECIE REG8(IT83XX_CEC_BASE + 0x05)
#define IT83XX_CEC_CECOPSTS REG8(IT83XX_CEC_BASE + 0x06)
#define IT83XX_CEC_CECOPSTS_CIP BIT(0)
#define IT83XX_CEC_CECOPSTS_AB BIT(1)
#define IT83XX_CEC_CECOPSTS_EB BIT(2)
#define IT83XX_CEC_CECOPSTS_DMS BIT(3)
#define IT83XX_CEC_CECOPSTS_IBE BIT(4)
#define IT83XX_CEC_CECOPSTS_CFLCTL BIT(5)
#define IT83XX_CEC_CECRH REG8(IT83XX_CEC_BASE + 0x07)

#ifdef CONFIG_ZEPHYR
#define IT83XX_IRQ_CEC 82
#endif

extern const struct cec_drv it83xx_cec_drv;

#ifdef TEST_BUILD
#include "driver/cec/it83xx_mock.h"
#endif

#endif /* __CROS_EC_DRIVER_CEC_IT83XX_H */
