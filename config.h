static struct mktimes
clocks[]                                = {
	{ "%u %d%m%y %H%M%S%z", NULL },
//	{ "%u %d%m%y %H%M%S%z", "" },
//	{ "%u %d%m%y %H%M%S%z", "Europe/Moscow" },
};

static struct getbattery
batteries[]                            = {
	{ "/sys/class/power_supply/BAT0",
	  "present", "energy_full_design", "energy_now", "power_now", "status" },
};

static struct gettemperature
sensors[]                               = {
	{ "/sys/devices/platform/coretemp.0", "temp1_input" },
};

