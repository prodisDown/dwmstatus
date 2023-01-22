/* Compare this values with VERSION_MAJOR and VERSION_MINOR from `config.mk`.
 * They are here not to piss you off but for that you didn't compile
 * incompatible versions of `config.h` and the rest of the code. */
#define CONFIG_VERSION_MAJOR 1
#define CONFIG_VERSION_MINOR 3

#define WIDGET_BUFLEN  32
#define STATUS_BUFLEN  256

static const char *status_begin = " ";
static const char *status_delim = " | ";
static const char *status_end = " ";

static const struct mktimes_arg farg_wallclock_localtime = {
	.fmt="%u %d%m%y %H%M%S%z", .tzname=NULL
};
static const struct getdiskusage_arg farg_fsavail_root = {
	.path="/", .name="/", .mode=1
};
static const struct getbattery_arg farg_power_BAT0 = {
	.dir="/sys/class/power_supply/BAT0",
	.present="present",
	.energy_max="energy_full_design",
	.energy_now="energy_now",
	.power_now="power_now",
	.status="status",
	.status_match_count = 3,
	.status_match = (const char*[]) {
		"Discharging\n", "Charging\n", "Full\n"
	},
	.status_output = (const char*[]) {
		"-", "+", "*"
	},
	.status_undef = "?"
};
static const struct gettemperature_arg farg_temp_CPU = {
	.dir="/sys/devices/platform/coretemp.0",
	.sensor="temp1_input"
};
static const struct getnetwork_arg farg_network_wlan0 = {
	.if_name = "wlan0",
	.vi_name = "W",
	.view_rates = 1
};
static const struct getnetwork_arg farg_network_wlan0_ap = {
	.if_name = "wlan0_ap",
	.vi_name = "AP",
	.view_rates = 1
};

static const Widget widget[] = {
	/* func, buflen, ctx_size, arg */
	{ getnetwork, 2*WIDGET_BUFLEN, sizeof(struct getnetwork_ctx), {.v = &farg_network_wlan0_ap} },
	{ getnetwork, 2*WIDGET_BUFLEN, sizeof(struct getnetwork_ctx), {.v = &farg_network_wlan0} },
	{ getdiskusage, 1*WIDGET_BUFLEN, 0, {.v = &farg_fsavail_root} },
	{ gettemperature, 1*WIDGET_BUFLEN, sizeof(struct gettemperature_ctx), {.v = &farg_temp_CPU} },
	{ getbattery, 1*WIDGET_BUFLEN, sizeof(struct getbattery_ctx), {.v = &farg_power_BAT0} },
	{ mktimes, 1*WIDGET_BUFLEN, 0, {.v = &farg_wallclock_localtime} },
};
static const Update update[] = {
	{ UP_WALLCLOCK, {.wallclock={1, 0}} },
	{ UP_WALLCLOCK, {.wallclock={2, 0}} },
	{ UP_WALLCLOCK, {.wallclock={5, 0}} },
};
static const short *(update_widgets[]) = {
	/* negative-terminated */
	(const short[]) {5, -1},
	(const short[]) {0, 1, 3, 4, -1},
	(const short[]) {2, -1},
};

