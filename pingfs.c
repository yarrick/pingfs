/*
 * Copyright (c) 2013-2015 Erik Ekman <yarrick@kryo.se>
 *
 * Permission to use, copy, modify, and/or distribute this software for any purpose
 * with or without fee is hereby granted, provided that the above copyright notice
 * and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <pwd.h>

#include "host.h"
#include "fs.h"
#include "net.h"
#include "chunk.h"

#include <arpa/inet.h>

#define DEFAULT_TIMEOUT_S 1

struct arginfo {
	char *hostfile;
	char *mountpoint;
	int num_args;
	int timeout;
};

enum {
	KEY_HELP,
	KEY_ASUSER,
	KEY_TIMEOUT,
};

static const struct fuse_opt pingfs_opts[] = {
	FUSE_OPT_KEY("-h",  KEY_HELP),
	FUSE_OPT_KEY("-u ", KEY_ASUSER),
	FUSE_OPT_KEY("-t ", KEY_TIMEOUT),
	FUSE_OPT_END,
};

static int read_hostnames(const char *hfile, struct gaicb **list[])
{
	int h = 0;
	FILE *file;

	if (strcmp("-", hfile) == 0) {
		file = stdin;
	} else {
		file = fopen(hfile, "r");
		if (!file) {
			perror("Failed to read file");
			return h;
		}
	}

	h = host_make_resolvlist(file, list);
	fclose(file);

	return h;
}

static int resolve_names(struct gaicb **list, int names, struct host **hosts)
{
	int ret;
	int hostcount;
	int i;

	fprintf(stderr, "Resolving %d hostnames... ", names);
	fflush(stderr);

	ret = getaddrinfo_a(GAI_WAIT, list, names, NULL);
	if (ret != 0) {
		fprintf(stderr, "Resolving failed: %s\n", gai_strerror(ret));
		return -1;
	}

	fprintf(stderr, "done.\n");

	hostcount = 0;
	for (i = 0; i < names; i++) {
		ret = gai_error(list[i]);
		if (ret) {
			fprintf(stderr, "Skipping %s: %s\n", list[i]->ar_name, gai_strerror(ret));
		} else {
			struct addrinfo *result = list[i]->ar_result;
			do {
				hostcount++;
				result = result->ai_next;
			} while (result);
		}
	}
	if (!hostcount) {
		fprintf(stderr, "No hosts found! Exiting\n");
		return -1;
	}

	*hosts = host_create(list, names);
	host_free_resolvlist(list, names);

	if (*hosts == NULL) {
		fprintf(stderr, "Failed creating list list, exiting\n");
		return -1;
	}

	return hostcount;
}

static void print_usage(char *progname)
{
	fprintf(stderr, "Usage: %s [options] hostfile mountpoint\n"
		"Options:\n"
		" -h           : Print this help and exit\n"
		" -u username  : Mount the filesystem as this user\n"
		" -t timeout   : Max time to wait for icmp reply "
			"(seconds, default 1)\n", progname);
}

static int pingfs_opt_proc(void *data, const char *arg, int key, struct fuse_args *outargs)
{
	struct arginfo *arginfo = (struct arginfo *) data;
	struct passwd *pw;
	int res;

	switch (key) {
	case FUSE_OPT_KEY_NONOPT:
		arginfo->num_args++;
		if (!arginfo->hostfile) {
			/* Get first non-option argument as hostfile */
			arginfo->hostfile = strdup(arg);
			return 0;
		} else if (!arginfo->mountpoint) {
			arginfo->mountpoint = strdup(arg);
		}
		break;
	case KEY_HELP:
		print_usage(outargs->argv[0]);
		exit(0);
	case KEY_ASUSER:
		pw = getpwnam(&arg[2]); /* Offset 2 to skip '-u' from arg */
		if (pw) {
			char userarg[64];
			snprintf(userarg, sizeof(userarg), "-ouid=%d,gid=%d", pw->pw_uid, pw->pw_gid);
			fuse_opt_add_arg(outargs, userarg);
			return 0;
		} else {
			fprintf(stderr, "Bad username given! Exiting\n");
			print_usage(outargs->argv[0]);
			exit(1);
		}
		break;
	case KEY_TIMEOUT:
		res = sscanf(arg, "-t%d", &arginfo->timeout);
		if (res == 1 && arginfo->timeout > 0 && arginfo->timeout < 60) {
			return 0;
		} else {
			fprintf(stderr, "Bad timeout given! Exiting\n");
			print_usage(outargs->argv[0]);
			exit(1);
		}
	}
	return 1;
}

int main(int argc, char **argv)
{
	struct gaicb **list;
	struct host *hosts = NULL;
	struct host *h;
	int hostnames;
	int host_count;
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
	struct arginfo arginfo;
	struct stat mountdir;
	int res;

	memset(&arginfo, 0, sizeof(arginfo));
	arginfo.timeout = DEFAULT_TIMEOUT_S;
	if (fuse_opt_parse(&args, &arginfo, pingfs_opts, pingfs_opt_proc) == -1) {
		fprintf(stderr, "Error parsing options!\n");
		print_usage(argv[0]);
		return EXIT_FAILURE;
	}

	if (arginfo.num_args != 2) {
		fprintf(stderr, "Need two arguments!\n");
		print_usage(argv[0]);
		return EXIT_FAILURE;
	}

	res = stat(arginfo.mountpoint, &mountdir);
	if (res) {
		perror("Failed to check mountpoint");
		return EXIT_FAILURE;
	}
	if (!S_ISDIR(mountdir.st_mode)) {
		fprintf(stderr, "Mountpoint must be a directory! Exiting\n");
		return EXIT_FAILURE;
	}
	free(arginfo.mountpoint);

	hostnames = read_hostnames(arginfo.hostfile, &list);
	free(arginfo.hostfile);
	if (!hostnames) {
		fprintf(stderr, "No hosts configured! Exiting\n");
		return EXIT_FAILURE;
	}

	if (net_open_sockets()) {
		fprintf(stderr, "No raw sockets opened. Got root?\n");
		return EXIT_FAILURE;
	}

	host_count = resolve_names(list, hostnames, &hosts);
	if (host_count < 0) {
		return EXIT_FAILURE;
	}

	host_count = host_evaluate(&hosts, host_count, arginfo.timeout);
	if (!host_count) {
		fprintf(stderr, "No host passed the test\n");
		return EXIT_FAILURE;
	}

	chunk_set_timeout(arginfo.timeout);

	host_use(hosts);

	/* Always run FUSE in foreground */
	fuse_opt_add_arg(&args, "-f");

	/* Run FUSE single threaded */
	fuse_opt_add_arg(&args, "-s");

	/* Default permissions handling, allow all users
	 * Directory is 775 so only root can use it anyway */
	fuse_opt_add_arg(&args, "-odefault_permissions,allow_other");

	/* Enable direct IO so we can do partial read/writes */
	fuse_opt_add_arg(&args, "-odirect_io");

	printf("Mounting filesystem\n");
	fuse_main(args.argc, args.argv, &fs_ops, NULL);

	/* Clean up */
	fuse_opt_free_args(&args);
	h = hosts;
	while (h) {
		struct host *host = h;
		h = h->next;
		free(host);
	}
	return EXIT_SUCCESS;
}
