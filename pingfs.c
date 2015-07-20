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

#include <arpa/inet.h>

static int sockv4;
static int sockv6;

int read_hostnames(char *hostfile, struct gaicb **list[])
{
	int h = 0;
	FILE *file;

	if (strcmp("-", hostfile) == 0) {
		file = stdin;
	} else {
		file = fopen(hostfile, "r");
		if (!file) {
			perror("Failed to read file");
			return h;
		}
	}

	h = host_make_resolvlist(file, list);
	fclose(file);

	return h;
}

int main(int argc, char **argv)
{
	struct gaicb **list;
	struct host *hosts;
	struct host *h;
	int hostnames;
	int hostcount;
	int i;
	int ret;

	if (argc != 2) {
		fprintf(stderr, "Expected one argument: path of hostname file\n");
		return EXIT_FAILURE;
	}

	hostnames = read_hostnames(argv[1], &list);
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
		fprintf(stderr, "Failed to open any raw sockets! Exiting\n");
		return EXIT_FAILURE;
	}

	fprintf(stderr, "Resolving %d hostnames... ", hostnames);
	fflush(stderr);

	ret = getaddrinfo_a(GAI_WAIT, list, hostnames, NULL);
	if (ret != 0) {
		fprintf(stderr, "Resolving failed: %s\n", gai_strerror(ret));
		return EXIT_FAILURE;
	}

	fprintf(stderr, "done.\n");

	hostcount = 0;
	for (i = 0; i < hostnames; i++) {
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
		return EXIT_FAILURE;
	}

	hosts = host_create(list, hostnames);
	host_free_resolvlist(list, hostnames);

	hostcount = host_evaluate(hosts, hostcount, sockv4, sockv6);

	int pkt_tx = 0;
	int pkt_rx = 0;
	int tot_ok = 0;
	h = hosts;
	while (h) {
		pkt_tx += h->tx_icmp;
		pkt_rx += h->rx_icmp;
		if (h->tx_icmp > 0 &&
			h->tx_icmp == h->rx_icmp) {

			tot_ok++;
		}
		h = h->next;
	}

	printf("\n%d of %d hosts responded correctly to all pings.\n", tot_ok, hostcount);
	printf("In total: %d packets sent, %d packets received.\n", pkt_tx, pkt_rx);

	h = hosts;
	while (h) {
		struct host *host = h;
		h = h->next;
		free(host);
	}
	return EXIT_SUCCESS;
}
