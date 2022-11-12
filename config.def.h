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

static const struct getnetwork_arg farg_network_wlan0 = {
	.if_name = "wlan0"
};



static const Widget widget[] = {
	{ getnetwork, WIDGET_BUFLEN*2, sizeof(struct getnetwork_ctx), {.v = &farg_network_wlan0} },
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
	(const short[]) {3, -1},
	(const short[]) {0, 1, 2, -1},
};

