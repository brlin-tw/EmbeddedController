


#include "charge_state.h"
#include "charger.h"
#include "charge_manager.h"
#include "chipset.h"
#include "cpu_power.h"
#include "customized_shared_memory.h"
#include "console.h"
#include "driver/sb_rmi.h"
#include "extpower.h"
#include "hooks.h"
#include "math_util.h"
#include "util.h"


#define CPRINTS(format, args...) cprints(CC_USBCHARGE, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_USBCHARGE, format, ## args)

static struct power_limit_details power_limit[FUNCTION_COUNT];
uint32_t ports_cost; /* TODO (0.9 * (total port cost)) */
bool manual_ctl;
static int battery_mwatt_type;
static int battery_mwatt_p3t;
static int battery_current_limit_mA;
static int target_func[TYPE_COUNT];
static int powerlimit_restore;

static int update_sustained_power_limit(uint32_t mwatt)
{
	uint32_t msgIn = 0;
	uint32_t msgOut;

	msgIn = mwatt;

	return sb_rmi_mailbox_xfer(SB_RMI_WRITE_SUSTAINED_POWER_LIMIT_CMD, msgIn, &msgOut);
}

static int update_flow_ppt_limit(uint32_t mwatt)
{
	uint32_t msgIn = 0;
	uint32_t msgOut;

	msgIn = mwatt;

	return sb_rmi_mailbox_xfer(SB_RMI_WRITE_FAST_PPT_LIMIT_CMD, msgIn, &msgOut);
}

static int update_slow_ppt_limit(uint32_t mwatt)
{
	uint32_t msgIn = 0;
	uint32_t msgOut;

	msgIn = mwatt;

	return sb_rmi_mailbox_xfer(SB_RMI_WRITE_SLOW_PPT_LIMIT_CMD, msgIn, &msgOut);
}

static int update_peak_package_power_limit(uint32_t mwatt)
{
	uint32_t msgIn = 0;
	uint32_t msgOut;

	msgIn = mwatt;

	return sb_rmi_mailbox_xfer(SB_RMI_WRITE_P3T_LIMIT_CMD, msgIn, &msgOut);
}

static void set_pl_limits(uint32_t spl, uint32_t fppt, uint32_t sppt, uint32_t p3t)
{
	update_sustained_power_limit(spl);
	update_flow_ppt_limit(fppt);
	update_slow_ppt_limit(sppt);
	update_peak_package_power_limit(p3t);
}

static void update_os_power_slider(int mode, int with_dc, int active_mpower)
{
	power_limit[FUNCTION_SLIDER].mwatt[TYPE_P3T] = battery_mwatt_p3t - POWER_DELTA - ports_cost;

	switch (mode) {
	case EC_DC_BEST_PERFORMANCE:
		power_limit[FUNCTION_SLIDER].mwatt[TYPE_SPL] = 30000;
		power_limit[FUNCTION_SLIDER].mwatt[TYPE_SPPT] = 35000;
		power_limit[FUNCTION_SLIDER].mwatt[TYPE_FPPT] =
			((battery_mwatt_type == BATTERY_55mW) ? 35000 : 41000);
		CPRINTS("DC BEST PERFORMANCE");
		break;
	case EC_DC_BALANCED:
		power_limit[FUNCTION_SLIDER].mwatt[TYPE_SPL] = 28000;
		power_limit[FUNCTION_SLIDER].mwatt[TYPE_SPPT] = 33000;
		power_limit[FUNCTION_SLIDER].mwatt[TYPE_FPPT] =
			((battery_mwatt_type == BATTERY_55mW) ? 35000 : 41000);
		CPRINTS("DC BALANCED");
		break;
	case EC_DC_BEST_EFFICIENCYE:
		power_limit[FUNCTION_SLIDER].mwatt[TYPE_SPL] = 15000;
		power_limit[FUNCTION_SLIDER].mwatt[TYPE_SPPT] = 20000;
		power_limit[FUNCTION_SLIDER].mwatt[TYPE_FPPT] = 30000;
		CPRINTS("DC BEST EFFICIENCYE");
		break;
	case EC_DC_BATTERY_SAVER:
		power_limit[FUNCTION_SLIDER].mwatt[TYPE_SPL] = 15000;
		power_limit[FUNCTION_SLIDER].mwatt[TYPE_SPPT] = 15000;
		power_limit[FUNCTION_SLIDER].mwatt[TYPE_FPPT] = 30000;
		CPRINTS("DC BATTERY SAVER");
		break;
	case EC_AC_BEST_PERFORMANCE:
		power_limit[FUNCTION_SLIDER].mwatt[TYPE_SPL] = 30000;
		power_limit[FUNCTION_SLIDER].mwatt[TYPE_SPPT] =
			(with_dc ? 35000 : (MIN(35000, ((active_mpower - 15000) * 9 / 10))));
		power_limit[FUNCTION_SLIDER].mwatt[TYPE_FPPT] = 53000;
		CPRINTS("AC BEST PERFORMANCE");
		break;
	case EC_AC_BALANCED:
		power_limit[FUNCTION_SLIDER].mwatt[TYPE_SPL] = 28000;
		power_limit[FUNCTION_SLIDER].mwatt[TYPE_SPPT] =
			(with_dc ? 33000 : (MIN(33000, ((active_mpower - 15000) * 9 / 10))));
		power_limit[FUNCTION_SLIDER].mwatt[TYPE_FPPT] =
			(with_dc ? 51000 : (MIN(51000, (active_mpower - 15000))));
		CPRINTS("AC BALANCED");
		break;
	case EC_AC_BEST_EFFICIENCYE:
		power_limit[FUNCTION_SLIDER].mwatt[TYPE_SPL] = (with_dc ? 15000 : 28000);
		power_limit[FUNCTION_SLIDER].mwatt[TYPE_SPPT] =
			(with_dc ? 25000 : (MIN(33000, ((active_mpower - 15000) * 9 / 10))));
		power_limit[FUNCTION_SLIDER].mwatt[TYPE_FPPT] =
			(with_dc ? 30000 : (MIN(51000, (active_mpower - 15000))));
		CPRINTS("AC BEST EFFICIENCYE");
		break;
	default:
		/* no mode, run power table */
		break;
	}
}

static void update_power_power_limit(int battery_percent, int active_mpower)
{
	if ((active_mpower < 55000)) {
		/* dc mode (active_mpower == 0) or AC < 55W */
		power_limit[FUNCTION_POWER].mwatt[TYPE_SPL] = 30000;
		power_limit[FUNCTION_POWER].mwatt[TYPE_SPPT] =
			battery_mwatt_type - POWER_DELTA - ports_cost;
		power_limit[FUNCTION_POWER].mwatt[TYPE_FPPT] =
			battery_mwatt_type - POWER_DELTA - ports_cost;
		power_limit[FUNCTION_POWER].mwatt[TYPE_P3T] =
			battery_mwatt_p3t - POWER_DELTA - ports_cost;
	} else if (battery_percent > 40) {
		/* ADP > 55W and Battery percentage > 40% */
		power_limit[FUNCTION_POWER].mwatt[TYPE_SPL] = 30000;
		power_limit[FUNCTION_POWER].mwatt[TYPE_SPPT] =
			MIN(43000, (active_mpower * 95 / 100)
				+ battery_mwatt_type - POWER_DELTA - ports_cost);
		power_limit[FUNCTION_POWER].mwatt[TYPE_FPPT] =
			MIN(53000, (active_mpower * 95 / 100)
				+ battery_mwatt_type - POWER_DELTA - ports_cost);
		power_limit[FUNCTION_POWER].mwatt[TYPE_P3T] =
			(active_mpower * 85 / 100) + battery_mwatt_type - POWER_DELTA - ports_cost;
	} else {
		/* ADP > 55W and Battery percentage <= 40% */
		power_limit[FUNCTION_POWER].mwatt[TYPE_SPL] = 30000;
		power_limit[FUNCTION_POWER].mwatt[TYPE_SPPT] =
			MIN(43000, (active_mpower * 95 / 100) - POWER_DELTA - ports_cost);
		power_limit[FUNCTION_POWER].mwatt[TYPE_FPPT] =
			MIN(53000, (active_mpower * 95 / 100) - POWER_DELTA - ports_cost);
		power_limit[FUNCTION_POWER].mwatt[TYPE_P3T] =
			(active_mpower * 85 / 100) - POWER_DELTA - ports_cost;
	}
}

static void update_dc_safety_power_limit(void)
{
	static int powerlimit_level;

	int new_mwatt;
	int delta;
	const struct batt_params *batt = charger_current_battery_params();
	int battery_current = batt->current;
	int battery_voltage = battery_dynamic[BATT_IDX_MAIN].actual_voltage;

	if (!powerlimit_restore) {
		/* restore to slider mode */
		power_limit[FUNCTION_SAFETY].mwatt[TYPE_SPL]
			= power_limit[FUNCTION_SLIDER].mwatt[TYPE_SPL];
		power_limit[FUNCTION_SAFETY].mwatt[TYPE_SPPT]
			= power_limit[FUNCTION_SLIDER].mwatt[TYPE_SPPT];
		power_limit[FUNCTION_SAFETY].mwatt[TYPE_FPPT]
			= power_limit[FUNCTION_SLIDER].mwatt[TYPE_FPPT];
		power_limit[FUNCTION_SAFETY].mwatt[TYPE_P3T]
			= power_limit[FUNCTION_SLIDER].mwatt[TYPE_P3T];
		powerlimit_restore = 1;
	} else {
		new_mwatt = power_limit[FUNCTION_SAFETY].mwatt[TYPE_SPL];
		/* start tuning PL by formate */
		/* discharge, value compare based on negative*/
		if (battery_current < battery_current_limit_mA) {
			/*
			 * reduce apu power limit by
			 * (1.2*((battery current - 3.57)* battery voltage)
			 * (mA * mV = mW / 1000)
			 */
			delta = (ABS(battery_current - battery_current_limit_mA)
				* battery_voltage) * 12 / 10 / 1000;
			new_mwatt = new_mwatt - delta;
			power_limit[FUNCTION_SAFETY].mwatt[TYPE_SPL]
				= MAX(new_mwatt, 15000);
			power_limit[FUNCTION_SAFETY].mwatt[TYPE_SPPT]
				= power_limit[FUNCTION_SAFETY].mwatt[TYPE_SPL];
			power_limit[FUNCTION_SAFETY].mwatt[TYPE_FPPT]
				= power_limit[FUNCTION_SAFETY].mwatt[TYPE_SPL];
			CPRINTF("batt ocp, delta: %d, new PL: %d\n",
				delta, power_limit[FUNCTION_SAFETY].mwatt[TYPE_SPL]);

			if (new_mwatt < 15000) {
				chipset_throttle_cpu(1);
				powerlimit_level = 1;
				CPRINTF("batt ocp, prochot\n");
			}
		} else if (battery_current > (battery_current_limit_mA * 9 / 10)) {
			/*
			 * increase apu power limit by
			 * (1.2*((battery current - 3.57)* battery voltage)
			 */
			if (powerlimit_level) {
				chipset_throttle_cpu(0);
				CPRINTF("batt ocp, recovery prochot\n");
				powerlimit_level = 0;
			} else {
				if (power_limit[FUNCTION_SAFETY].mwatt[TYPE_SPL]
					== power_limit[FUNCTION_SLIDER].mwatt[TYPE_SPL]) {
					powerlimit_restore = 0;
					return;
				}
				delta = (ABS(battery_current - battery_current_limit_mA)
					* battery_voltage) * 12 / 10 / 1000;
				new_mwatt = new_mwatt + delta;

				power_limit[FUNCTION_SAFETY].mwatt[TYPE_SPL] = MIN(new_mwatt,
					power_limit[FUNCTION_SLIDER].mwatt[TYPE_SPL]);
				power_limit[FUNCTION_SAFETY].mwatt[TYPE_SPPT]
					= power_limit[FUNCTION_SAFETY].mwatt[TYPE_SPL];
				power_limit[FUNCTION_SAFETY].mwatt[TYPE_FPPT]
					= power_limit[FUNCTION_SAFETY].mwatt[TYPE_SPL];
				CPRINTF("batt ocp recover, delta: %d, new PL: %d\n",
					delta, power_limit[FUNCTION_SAFETY].mwatt[TYPE_SPL]);
			}
		}
	}
}

void update_soc_power_limit(bool force_update, bool force_no_adapter)
{
	static uint32_t old_sustain_power_limit;
	static uint32_t old_fast_ppt_limit;
	static uint32_t old_slow_ppt_limit;
	static uint32_t old_p3t_limit;
	static int old_slider_mode = EC_DC_BALANCED;
	int mode = *host_get_memmap(EC_MEMMAP_POWER_SLIDE);
	int active_mpower = charge_manager_get_power_limit_uw() / 1000;
	bool with_dc = ((battery_is_present() == BP_YES) ? true : false);
	int battery_percent = charge_get_percent();

	if (force_no_adapter || (!extpower_is_present())) {
		active_mpower = 0;
		if (mode > EC_DC_BATTERY_SAVER)
			mode = mode << 4;
	}

	if (old_slider_mode != mode) {
		old_slider_mode = mode;
		update_os_power_slider(mode, with_dc, active_mpower);
	}

	update_power_power_limit(battery_percent, active_mpower);

	if (active_mpower == 0)
		update_dc_safety_power_limit();
	else {
		power_limit[FUNCTION_SAFETY].mwatt[TYPE_SPL] = 0;
		power_limit[FUNCTION_SAFETY].mwatt[TYPE_SPPT] = 0;
		power_limit[FUNCTION_SAFETY].mwatt[TYPE_FPPT] = 0;
		power_limit[FUNCTION_SAFETY].mwatt[TYPE_P3T] = 0;
		powerlimit_restore = 0;
	}

	/* choose the lowest one */
	for (int item = TYPE_SPL; item < TYPE_COUNT; item++) {
		/* use slider as default */
		target_func[item] = FUNCTION_SLIDER;
		for (int func = FUNCTION_DEFAULT; func < FUNCTION_COUNT; func++) {
			if (power_limit[func].mwatt[item] < 1)
				continue;
			if (power_limit[target_func[item]].mwatt[item]
				> power_limit[func].mwatt[item])
				target_func[item] = func;
		}
	}

	if (power_limit[target_func[TYPE_SPL]].mwatt[TYPE_SPL] != old_sustain_power_limit
		|| power_limit[target_func[TYPE_FPPT]].mwatt[TYPE_FPPT] != old_fast_ppt_limit
		|| power_limit[target_func[TYPE_SPPT]].mwatt[TYPE_SPPT] != old_slow_ppt_limit
		|| power_limit[target_func[TYPE_P3T]].mwatt[TYPE_P3T] != old_p3t_limit
		|| force_update) {
		/* only set PL when it is changed */
		old_sustain_power_limit = power_limit[target_func[TYPE_SPL]].mwatt[TYPE_SPL];
		old_slow_ppt_limit = power_limit[target_func[TYPE_SPPT]].mwatt[TYPE_SPPT];
		old_fast_ppt_limit = power_limit[target_func[TYPE_FPPT]].mwatt[TYPE_FPPT];
		old_p3t_limit = power_limit[target_func[TYPE_P3T]].mwatt[TYPE_P3T];

		CPRINTF("Change SOC Power Limit: SPL %dmW, sPPT %dmW, fPPT %dmW p3T %dmW\n",
			old_sustain_power_limit, old_slow_ppt_limit,
			old_fast_ppt_limit, old_p3t_limit);
		set_pl_limits(old_sustain_power_limit, old_fast_ppt_limit,
			old_slow_ppt_limit, old_p3t_limit);
	}
}

void update_soc_power_limit_hook(void)
{
	if (!manual_ctl)
		update_soc_power_limit(false, false);
}
DECLARE_HOOK(HOOK_SECOND, update_soc_power_limit_hook, HOOK_PRIO_DEFAULT);
DECLARE_HOOK(HOOK_AC_CHANGE, update_soc_power_limit_hook, HOOK_PRIO_DEFAULT);

static void initial_soc_power_limit(void)
{
	char *str = "FRANGWAT01";

	battery_mwatt_type =
		(strncmp(battery_static[BATT_IDX_MAIN].model_ext, str, 10) ?
		BATTERY_55mW : BATTERY_61mW);
	battery_mwatt_p3t =
		((battery_mwatt_type == BATTERY_55mW) ? 100000 : 90000);
	battery_current_limit_mA =
		((battery_mwatt_type == BATTERY_55mW) ? -3570 : -3920);

	/* initial slider table to battery balance as default */
	power_limit[FUNCTION_SLIDER].mwatt[TYPE_SPL] = 28000;
	power_limit[FUNCTION_SLIDER].mwatt[TYPE_SPPT] = 33000;
	power_limit[FUNCTION_SLIDER].mwatt[TYPE_FPPT] =
		((battery_mwatt_type == BATTERY_55mW) ? 35000 : 41000);
	power_limit[FUNCTION_SLIDER].mwatt[TYPE_P3T] = battery_mwatt_p3t - POWER_DELTA - ports_cost;
}
DECLARE_HOOK(HOOK_INIT, initial_soc_power_limit, HOOK_PRIO_INIT_I2C);

static int cmd_cpupower(int argc, const char **argv)
{
	uint32_t spl, fppt, sppt, p3t;
	char *e;

	CPRINTF("Now SOC Power Limit:\n FUNC = %d, SPL %dmW,\n",
		target_func[TYPE_SPL], power_limit[target_func[TYPE_SPL]].mwatt[TYPE_SPL]);
	CPRINTF("FUNC = %d, fPPT %dmW,\n FUNC = %d, sPPT %dmW,\n FUNC = %d, p3T %dmW\n",
		target_func[TYPE_SPPT], power_limit[target_func[TYPE_SPPT]].mwatt[TYPE_SPPT],
		target_func[TYPE_FPPT], power_limit[target_func[TYPE_FPPT]].mwatt[TYPE_FPPT],
		target_func[TYPE_P3T], power_limit[target_func[TYPE_P3T]].mwatt[TYPE_P3T]);

	if (argc >= 2) {
		if (!strncmp(argv[1], "auto", 4)) {
			manual_ctl = false;
			CPRINTF("Auto Control");
			update_soc_power_limit(false, false);
		}
		if (!strncmp(argv[1], "manual", 6)) {
			manual_ctl = true;
			CPRINTF("Manual Control");
		}
		if (!strncmp(argv[1], "table", 5)) {
			CPRINTF("Table Power Limit:\n");
			for (int i = FUNCTION_DEFAULT; i < FUNCTION_COUNT; i++) {
				CPRINTF("function %d, SPL %dmW, fPPT %dmW, sPPT %dmW, p3T %dmW\n",
					i, power_limit[i].mwatt[TYPE_SPL],
					power_limit[i].mwatt[TYPE_FPPT],
					power_limit[i].mwatt[TYPE_SPPT],
					power_limit[i].mwatt[TYPE_P3T]);
			}
		}
	}

	if (argc >= 5) {
		spl = strtoi(argv[1], &e, 0);
		if (*e)
			return EC_ERROR_PARAM1;
		fppt = strtoi(argv[2], &e, 0);
		if (*e)
			return EC_ERROR_PARAM2;

		sppt = strtoi(argv[2], &e, 0);
		if (*e)
			return EC_ERROR_PARAM3;

		p3t = strtoi(argv[2], &e, 0);
		if (*e)
			return EC_ERROR_PARAM4;

		set_pl_limits(spl, fppt, sppt, p3t);
	}
	return EC_SUCCESS;

}
DECLARE_CONSOLE_COMMAND(cpupower, cmd_cpupower,
			"cpupower spl fppt sppt p3t (unit mW)",
			"Set/Get the cpupower limit");
