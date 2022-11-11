#define WIDGET_BUFLEN  32
#define STATUS_BUFLEN  256

static const char *status_begin = " ";
static const char *status_delim = " | ";
static const char *status_end = " ";



static const struct mktimes_arg farg_wallclock_localtime = {
	.fmt="%u %d%m%y %H%M%S%z",
	.tzname=NULL
};

static const struct getbattery_arg farg_power_BAT0 = {
	.dir="/sys/class/power_supply/BAT0",
	.n_present="present",
	.n_energy_design="energy_full_design",
	.n_energy_now="energy_now",
	.n_power_now="power_now",
	.n_status="status",
	.status_txt = (const char*[]) {
	/* undefined   */ "?",
	/* charging    */ "+",
	/* discharging */ "-"
	}
};

static const struct gettemperature_arg farg_temp_CPU = {
	.dir="/sys/devices/platform/coretemp.0",
	.n_sensor="temp1_input"
};



static const Widget widget[] = {
	{ gettemperature, WIDGET_BUFLEN, sizeof(struct gettemperature_ctx), {.v = &farg_temp_CPU} },
	{ getbattery, WIDGET_BUFLEN, sizeof(struct getbattery_ctx), {.v = &farg_power_BAT0} },
	{ mktimes, WIDGET_BUFLEN, 0, {.v = &farg_wallclock_localtime} },
};

static const Update update[] = {
	{ UP_WALLCLOCK, {.tp={1,0}} },
	{ UP_WALLCLOCK, {.tp={2,0}} },
};

static const short *(update_widgets[]) = {
	/* negative-terminated */
	(const short[]) {2, -1},
	(const short[]) {0, 1, -1},
};




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

