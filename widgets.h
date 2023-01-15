/* functions in format `ssize_t (char *restrict, size_t, void *, const Arg)` */
ssize_t mktimes(char *restrict, size_t, void *, const Arg);
ssize_t getbattery(char *restrict, size_t, void *, const Arg);
ssize_t getdiskusage(char *restrict, size_t, void *, const Arg);
ssize_t getnetwork(char *restrict, size_t, void *, const Arg);
ssize_t gettemperature(char *restrict, size_t, void *, const Arg);

/* widget-argument structures */
struct mktimes_arg {
	const char *fmt;
	const char *tzname;
};
struct getbattery_arg {
	const char *dir;
	const char *n_present,
		   *n_energy_design,
		   *n_energy_now,
		   *n_power_now,
		   *n_status;
	const char *(*status_txt);
};
struct getdiskusage_arg {
	int mode; /* 0: file size; 1: fs free; 2: fs used/total; */
	const char *path;
	const char *name; /* NULL: don't print name; else: use name */
};
struct getnetwork_arg {
	bool view_rates;
	const char *if_name;
	const char *vi_name;
};
struct gettemperature_arg {
	const char *dir;
	const char *n_sensor;
};

/* widget-context structures */
struct gettemperature_ctx {
	int fd_hwmon;
};
struct getnetwork_ctx {
	uint64_t last_rx,
		 last_tx;
	struct timespec last;
};
struct getbattery_ctx {
	int fd_dir;
};

/* code */

ssize_t mktimes(char *restrict buf, size_t buflen, void *ctx, const Arg arg)
{
	(void)ctx;
	struct mktimes_arg *s = (struct mktimes_arg *)arg.v;

	struct timespec ts;
	struct tm tm;

	/* If ct->tzname == NULL, leave TZ undefined and use system localtime.
	 */
	unsetenv("TZ");
	if (s->tzname != NULL)
		setenv("TZ", s->tzname, 1);
	tzset();

	clock_gettime(CLOCK_REALTIME, &ts);
	localtime_r(&ts.tv_sec, &tm);

	size_t ws = strftime(buf, buflen, s->fmt, &tm);
	if (0 >= ws)
		goto error;

	return (ssize_t)ws;

error:
	return -1;
}

ssize_t getbattery(char *restrict buf, size_t buflen, void *ctx, const Arg arg)
{
	struct getbattery_arg *s = (struct getbattery_arg *)arg.v;
	struct getbattery_ctx *c = (struct getbattery_ctx *)ctx;

	short status_idx;
	long energy_design = -1,
	     energy_now = -1,
	     power_now = -1;

	/* Open battery directory if it wasn't */
	if (c->fd_dir == 0)
		c->fd_dir = open(s->dir, O_RDONLY|O_DIRECTORY);
	if (0 > c->fd_dir) {
		goto error;
	}

	/* BAT: present */
	{
		char rbuf;
		int fd_present = openat(c->fd_dir, s->n_present, O_RDONLY);
		if (0 > fd_present) {
			goto error;
		}

		read(fd_present, &rbuf, 1);
		close(fd_present);

		if (rbuf != '1') {
			sprintf(buf, "no battery");
			return 11;
		}
	}
	/* BAT: energy designed capacity */
	{
		char rbuf[32] = {0};
		int fd_energy_design = openat(c->fd_dir,
		                              s->n_energy_design,
		                              O_RDONLY);
		if (0 > fd_energy_design) {
			goto error;
		}

		read(fd_energy_design, rbuf, COUNT(rbuf));
		close(fd_energy_design);

		if (rbuf[COUNT(rbuf)-1] != '\0') {
			goto error;
		}
		if (0 >= sscanf(rbuf, "%ld", &energy_design)) {
			goto error;
		}
	}
	/* BAT: energy remaining capacity */
	{
		char rbuf[32] = {0};
		int fd_energy_now = openat(c->fd_dir,
		                              s->n_energy_now,
		                              O_RDONLY);
		if (0 > fd_energy_now) {
			goto error;
		}

		read(fd_energy_now, rbuf, COUNT(rbuf));
		close(fd_energy_now);

		if (rbuf[COUNT(rbuf)-1] != '\0') {
			goto error;
		}
		if (0 >= sscanf(rbuf, "%ld", &energy_now)) {
			goto error;
		}
	}
	/* BAT: current power consumption */
	{
		char rbuf[32] = {0};
		int fd_power_now = openat(c->fd_dir,
		                              s->n_power_now,
		                              O_RDONLY);
		if (0 > fd_power_now) {
			goto error;
		}

		read(fd_power_now, rbuf, COUNT(rbuf));
		close(fd_power_now);

		if (rbuf[COUNT(rbuf)-1] != '\0') {
			goto error;
		}
		if (0 >= sscanf(rbuf, "%ld", &power_now)) {
			goto error;
		}
	}
	/* BAT: current status of the battery */
	{
		char rbuf[32] = {0};
		int fd_status = openat(c->fd_dir,
		                              s->n_status,
		                              O_RDONLY);
		if (0 > fd_status) {
			goto error;
		}

		read(fd_status, rbuf, COUNT(rbuf));
		close(fd_status);

		if (rbuf[COUNT(rbuf)-1] != '\0')
			goto error;

		if (!strcmp(rbuf,
				"Discharging\n"))
			status_idx=2;
		else if (!strcmp(rbuf,
				"Charging\n"))
			status_idx=1;
		else
			status_idx=0;
	}

	if (0 > energy_now || 0 > energy_design)
		goto error;

	int psize = snprintf(buf, buflen, "%s %.2f %.2f",
	        s->status_txt[status_idx],
	        ((float)energy_now / (float)energy_design) * 100.0,
	        (float)power_now / 1e6f);
	if (psize >= buflen)
		goto error;

	return (ssize_t)psize;
error:
	return -1;
}

ssize_t getdiskusage(char *restrict buf, size_t buflen, void *ctx, const Arg arg)
{
	struct getdiskusage_arg *s = (struct getdiskusage_arg *)arg.v;
	(void)ctx;
	const char unit[] = {'B','K','M','G','T'};
	uint8_t u=0;
	size_t cur=0;
	if (!s->path) goto error;

	if (s->name) {
		int rc=snprintf((buf+cur), (buflen-cur), "%s: ", s->name);
		if ((size_t)rc >= buflen-cur) goto error;
		cur += (size_t)rc;
	}

	switch (s->mode) {
	case 0:
		;
		struct stat st;
		{
			int rv=stat(s->path, &st);
			if (0 > rv)
				goto norm;
		}
		float fsz=(float)st.st_size;
		for(u=0; fsz>=1000 && u<COUNT(unit)-1; ++u, fsz/=1024);
		;
		{
			int rc=snprintf((buf+cur), (buflen-cur),
					u>0?"%.1f%c":"%.0f",
					fsz, unit[u]);
			if ((size_t)rc >= buflen-cur) goto error;
			cur += (size_t)rc;
		}
		break;
	case 1:
		;
		struct statvfs stfs;
		{
			int rv=statvfs(s->path, &stfs);
			if (0 > rv)
				goto norm;
		}
		/* show disk space available for unprivileged users (like `df -h`)*/
		float fssz_av=(float)stfs.f_frsize * (float)stfs.f_bavail;

		for(u=0; fssz_av>=1000 && u<COUNT(unit)-1; ++u, fssz_av/=1024);
		;
		{
			int rc=snprintf((buf+cur), (buflen-cur),
					u>0?"%.1f%c":"%.0f",
					fssz_av, unit[u]);
			if ((size_t)rc >= buflen-cur) goto error;
			cur += (size_t)rc;
		}
		break;
	}

norm:
	return (ssize_t)cur;

error:
	return -1;
}

ssize_t getnetwork(char *restrict buf, size_t buflen, void *ctx, const Arg arg)
{
	struct getnetwork_arg *s = (struct getnetwork_arg *)arg.v;
	struct getnetwork_ctx *c = (struct getnetwork_ctx *)ctx;
	size_t cur=0;

	struct ifaddrs *ial=NULL;
	struct ifaddrs *ia_packet=NULL,
		       *ia_inet=NULL;

	/* interface device name */
	{
		int rc=snprintf(buf, buflen, "%s:", s->vi_name != NULL ?
				s->vi_name : s->if_name);
		if ((size_t)rc >= buflen) goto error;
		cur += (size_t)rc;
	}

	if (0 > getifaddrs(&ial))
		goto error;

	struct ifaddrs *ia;
	for(ia=ial; NULL!=ia; ia=ia->ifa_next) {
		if (strcmp(s->if_name, ia->ifa_name)) continue;
		if (NULL == ia->ifa_addr) continue;

		switch(ia->ifa_addr->sa_family) {
		case AF_PACKET:
			ia_packet=ia;
			break;
		case AF_INET:
			ia_inet=ia;
			break;
		}
	}

	/* not found link device */
	if (!ia_packet) {
		goto norm;
	}

	/* interface is down */
	if (!(IFF_UP & ia_packet->ifa_flags)) {
		int rc=snprintf((buf+cur), (buflen-cur), " down");
		if ((size_t)rc >= buflen-cur) goto error;
		cur += (size_t)rc;
		goto norm;
	}

	/* IP-address */
	if (!ia_inet || !ia_inet->ifa_addr) {
		int rc=snprintf((buf+cur), (buflen-cur), " up");
		if ((size_t)rc >= buflen-cur) goto error;
		cur += (size_t)rc;
	} else {
		struct sockaddr_in *s_in = (struct sockaddr_in *)(ia_inet->ifa_addr);
		char *ip = inet_ntoa(s_in->sin_addr);
		int rc=snprintf((buf+cur), (buflen-cur), " %s", ip);
		if ((size_t)rc >= buflen-cur) goto error;
		cur += (size_t)rc;
	}

	/* rx/tx rates */
	if (s->view_rates && ia_packet->ifa_data) {
		struct rtnl_link_stats *stats = ia_packet->ifa_data;
		uint32_t diff_rx, diff_tx;
		uint32_t tdiv;

		diff_rx = stats->rx_bytes - c->last_rx;
		diff_tx = stats->tx_bytes - c->last_tx;
		c->last_rx = stats->rx_bytes;
		c->last_tx = stats->tx_bytes;

		{
			struct timespec lag, now;
			clock_gettime(CLOCK_REALTIME, &now);
			timespec_sub(&lag, &now, &c->last);
			memcpy(&c->last, &now, sizeof(c->last));
			if (0 > lag.tv_sec) goto norm;
			tdiv = (uint32_t)lag.tv_sec + (uint32_t)(lag.tv_nsec/1000000000);
			if (0.1 >= tdiv) goto norm;
		}

		uint8_t prx, ptx;
		uint32_t rx, tx;
		for(prx=0, rx=diff_rx / tdiv;
				rx >= 1024;
				++prx, rx/=1024);
		for(ptx=0, tx=diff_tx / tdiv;
				tx >= 1024;
				++ptx, tx/=1024);

		const char *rxfs, *txfs;
		rxfs = diff_rx>0 ? " %x%x" : " ..";
		txfs = diff_tx>0 ? " %x%x" : " ..";

		{
			int rc=snprintf((buf+cur), (buflen-cur),
					rxfs, (uint8_t)(rx/64), prx);
			if ((size_t)rc >= buflen-cur) goto error;
			cur += (size_t)rc;
		}
		{
			int rc=snprintf((buf+cur), (buflen-cur),
					txfs, (uint8_t)(tx/64), ptx);
			if ((size_t)rc >= buflen-cur) goto error;
			cur += (size_t)rc;
		}
	}

norm:;
	freeifaddrs(ial);
	return (ssize_t)cur;

error:;
	if (ial)
		freeifaddrs(ial);
	buf[0] = '\0';
	return -1;
}

ssize_t gettemperature(char *restrict buf, const size_t buflen, void *ctx, const Arg arg)
{
	struct gettemperature_arg *s = (struct gettemperature_arg *)arg.v;
	struct gettemperature_ctx *c = (struct gettemperature_ctx *)ctx;

	/* Find out what hwmon to watch */
	if (0 >= c->fd_hwmon) {
		DIR *d_hwmons;
		struct dirent *e_hwmons;
		char p_hwmons[PATH_MAX];

		sprintf(p_hwmons, "%s/hwmon", s->dir);
		d_hwmons = opendir(p_hwmons);
		if (NULL == d_hwmons)
		{
			c->fd_hwmon = -1;
			goto error;
		}

		while (NULL != (e_hwmons = readdir(d_hwmons)))
		{
			if (e_hwmons->d_type == DT_DIR
			  && e_hwmons->d_name[0] != '.')
				break;
		}
		if (e_hwmons == NULL) {
			c->fd_hwmon = -1;
			closedir(d_hwmons);
			goto error;
		}

		c->fd_hwmon = openat(dirfd(d_hwmons),
		                     e_hwmons->d_name,
		                     O_RDONLY|O_DIRECTORY);
		closedir(d_hwmons);
	}
	if (c->fd_hwmon < 0)
		goto error;

	/* Read sensor */
	int psize;
	{
		char rbuf[21] = {0};
		int fd_sensor = openat(c->fd_hwmon, s->n_sensor, O_RDONLY);
		if (0 > fd_sensor)
			goto error;

		read(fd_sensor, rbuf, 21);
		close(fd_sensor);

		if (rbuf[20] != '\0')
			goto error;
		psize = snprintf(buf, buflen,
		                 "%2.0f°C",
		                 atof(rbuf) / 1e3f);
	}
	if (psize >= buflen)
		goto error;

	return psize;
error:;
	buf[0] = '\0';
	return -1;
}
