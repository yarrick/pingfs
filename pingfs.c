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
#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>

#include "icmp.h"
#include "host.h"
#include "fs.h"

#include <arpa/inet.h>

static int sockv4;
static int sockv6;

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

	return hostcount;
}

static void print_usage(char *progname)
{
	fprintf(stderr, "Usage: %s hostfile mountpoint\n", progname);
}

static char *hostfile;
static int num_args;

static int pingfs_opt_proc(void *data, const char *arg, int key, struct fuse_args *outargs)
{
	/* Get first non-option argument as hostfile */
	if (key == FUSE_OPT_KEY_NONOPT) {
		num_args++;
		if (hostfile == NULL) {
			hostfile = strdup(arg);
			return 0;
		}
	}
	return 1;
}

static const struct fuse_operations fs_ops = {
	.getattr = fs_getattr,
	.chmod = fs_chmod,
	.mknod = fs_mknod,
	.unlink = fs_unlink,
	.readdir = fs_readdir,
};

int main(int argc, char **argv)
{
	struct gaicb **list;
	struct host *hosts = NULL;
	struct host *h;
	int hostnames;
	int host_count;
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

	if (fuse_opt_parse(&args, NULL, NULL, pingfs_opt_proc) == -1) {
		fprintf(stderr, "Error parsing options!\n");
		print_usage(argv[0]);
		return EXIT_FAILURE;
	}

	if (num_args != 2) {
		fprintf(stderr, "Need two arguments!\n");
		print_usage(argv[0]);
		return EXIT_FAILURE;
	}

	hostnames = read_hostnames(hostfile, &list);
	free(hostfile);
	if (!hostnames) {
		fprintf(stderr, "No hosts configured! Exiting\n");
		return EXIT_FAILURE;
	}

	sockv4 = open_icmpv4_socket();
	if (sockv4 < 0) {
		perror("Failed to open IPv4 socket");
	}
	sockv6 = open_icmpv6_socket();
	if (sockv6 < 0) {
		perror("Failed to open IPv6 socket");
	}
	if (sockv4 < 0 && sockv6 < 0) {
		fprintf(stderr, "No raw sockets opened. Got root?\n");
		return EXIT_FAILURE;
	}

	host_count = resolve_names(list, hostnames, &hosts);
	if (host_count < 0) {
		return EXIT_FAILURE;
	}

	host_count = host_evaluate(&hosts, host_count, sockv4, sockv6);
	if (!host_count) {
		fprintf(stderr, "No host passed the test\n");
		return EXIT_FAILURE;
	}

	/* Always run FUSE in foreground */
	fuse_opt_add_arg(&args, "-f");

	printf("Mounting filesystem\n");
	fuse_main(args.argc, args.argv, &fs_ops);

	/* Clean up */
	fuse_opt_free_args(&args);
	h = hosts;
	while (h) {
		struct host *host = h;
		h = h->next;
		free(host);
	}
	fs_cleanup();
	return EXIT_SUCCESS;
}
