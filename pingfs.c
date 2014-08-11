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
	struct host *hostlist;
	int hostnames;
	int hosts;
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

	hosts = 0;
	for (i = 0; i < hostnames; i++) {
		ret = gai_error(list[i]);
		if (ret) {
			fprintf(stderr, "Skipping %s: %s\n", list[i]->ar_name, gai_strerror(ret));
		} else {
			struct addrinfo *result = list[i]->ar_result;
			do {
				hosts++;
				result = result->ai_next;
			} while (result);
		}
	}
	if (!hosts) {
		fprintf(stderr, "No hosts found! Exiting\n");
		return EXIT_FAILURE;
	}

	hostlist = host_create(list, hostnames, hosts);
	host_free_resolvlist(list, hostnames);

	hosts = host_evaluate(hostlist, hosts, sockv4, sockv6);

	int pkt_tx = 0;
	int pkt_rx = 0;
	int tot_ok = 0;
	for (i = 0; i < hosts; i++) {
		pkt_tx += hostlist[i].tx_icmp;
		pkt_rx += hostlist[i].rx_icmp;
		if (hostlist[i].tx_icmp > 0 &&
			hostlist[i].tx_icmp == hostlist[i].rx_icmp) {
		tot_ok++;
		}
	}

	free(hostlist);
	printf("\n%d of %d hosts responded correctly to all pings.\n", tot_ok, hosts);
	printf("In total: %d packets sent, %d packets received.\n", pkt_tx, pkt_rx);

	return EXIT_SUCCESS;
}
