/*
 * deadman is a software deadman's switch.
 * Copyright (C) 2019 Esote
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#define _POSIX_C_SOURCE	200112L	/* setenv, unsetenv */
#define _XOPEN_SOURCE		/* strptime */

#include <sys/stat.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define FMT	"%Y-%m-%dT%H:%M:%S"
#define USAGE	"usage: %s time cmd [arg...]"

#ifdef _NSIG
#define NUMSIGS	_NSIG
#elif defined NSIG
#define NUMSIGS	NSIG
#elif defined SIGRTMAX
#define NUMSIGS	(SIGRTMAX+1) /* assuming rt signals come after standard ones */
#else
#define NUMSIGS	65
#endif

static time_t	parse(char const *const);
static time_t	mktime_tz(struct tm *const, char const *const);
static char *	strdup(char const *const);
static void	mkdaemon(void);
static int	exec(char *const []);

int
main(int const argc, char *const argv[])
{
	struct timespec req;
	time_t t;

	if (argc < 2) {
		errx(1, "missing time\n"USAGE, argc == 0 ? "deadman" : argv[0]);
	} else if (argc < 3) {
		errx(1, "missing command\n"USAGE, argv[0]);
	}

	if ((t = parse(argv[1])) == -1) {
		if (errno == EOVERFLOW) {
			err(1, "time parsing failed");
		} else {
			errx(1, "time parsing failed");
		}
	}

	if (strlen(argv[2]) == 0 || argv[2][0] != '/') {
		errx(1, "command must be an absolute path");
	}

	req.tv_sec = 1;
	req.tv_nsec = 0;

	if (t <= time(NULL)) {
		warnx("time <= current, will exec immediately");
	}

	mkdaemon();

	while (1) {
		if (t <= time(NULL)) {
			return exec(argv+2);
		}

		nanosleep(&req, NULL);
	}

	return EXIT_SUCCESS;
}

static time_t
parse(char const *const str)
{
	struct tm parsed = {0};
	char const *end;

	if ((end = strptime(str, FMT, &parsed)) == NULL || *end != '\0') {
		errx(1, "malformed time");
	}

	return mktime_tz(&parsed, "UTC");
}

static time_t
mktime_tz(struct tm *const tm, char const *const tz)
{
	char *old;
	char const *tmp;
	time_t t;

	if ((tmp = getenv("TZ")) != NULL && (old = strdup(tmp)) == NULL) {
		err(1, "strdup getenv");
	}

	if (setenv("TZ", tz, 1) == -1) {
		err(1, "setenv TZ to tz");
	}

	t = mktime(tm);

	if (tmp == NULL) {
		if (unsetenv("TZ") == -1) {
			err(1, "unsetenv TZ");
		}
	} else {
		if (setenv("TZ", old, 1) == -1) {
			err(1, "setenv TZ reset to old");
		}

		free(old);
	}

	return t;
}

static char *
strdup(char const *const s)
{
	size_t l;
	char *d;

	l = strlen(s);
	d = malloc(l + 1);

	if (d == NULL) {
		return NULL;
	}

	return memcpy(d, s, l + 1);
}

static void
mkdaemon(void)
{
	long i;
	pid_t p;

	if ((p = fork()) == -1) {
		err(1, "daemon first fork");
	} else if (p > 0) {
		exit(EXIT_SUCCESS);
	}

	if (setsid() == -1) {
		err(1, "setsid");
	}

	for (i = 1; i < NUMSIGS; ++i) {
		/* Ignore SIG_ERR. */
		signal((int)i, SIG_IGN);
	}

	if ((p = fork()) == -1) {
		err(1, "daemon second fork");
	} else if (p > 0) {
		exit(EXIT_SUCCESS);
	}

	umask(0);

	if (chdir("/") == -1) {
		err(1, "chdir");
	}

	i = sysconf(_SC_OPEN_MAX);

	if (i == -1) {
		errx(1, "sysconf _SC_OPEN_MAX");
	} else if (i > INT_MAX) {
		i = INT_MAX;
		warnx("_SC_OPEN_MAX exceeds max fd value");
	}

	for (; i >= 0; --i) {
		if (close((int)i) == -1 && errno != EBADF
			&& i >= STDERR_FILENO) {
			warn("closing fd %ld failed", i);
		}
	}

	/* Ignore failed fd closes going forward. */
	errno = 0;
}

static int
exec(char *const argv[])
{
	pid_t p;

	if ((p = fork()) == -1 || (p == 0 && execvp(argv[0], argv) == -1)) {
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
