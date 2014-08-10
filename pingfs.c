#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <sys/param.h>

#include "icmp.h"
#include "pingfs.h"

static const struct addrinfo addr_request = {
	.ai_family = AF_UNSPEC,
	.ai_socktype = SOCK_RAW,
};

static uint8_t eval_payload[1024];

static int sockv4;
static int sockv6;

static int read_hostnames(char *hostfile, struct gaicb **list)
{
	char hostname[300];
	int h = 0;
	int listsize = 32;
	FILE *file;

	struct gaicb *gais;

	if (strcmp("-", hostfile) == 0) {
		file = stdin;
	} else {
		file = fopen(hostfile, "r");
		if (!file) {
			perror("Failed to read file");
			return h;
		}
	}

	gais = malloc(listsize * sizeof(struct gaicb));
	for (;;) {
		int res;
		memset(hostname, 0, sizeof(hostname));
		res = fscanf(file, "%256s", hostname);
		if (res == EOF) break;
		if (h == listsize) {
			listsize *= 2;
			gais = realloc(gais, listsize * sizeof(struct gaicb));
		}
		memset(&gais[h], 0, sizeof(struct gaicb));
		gais[h].ar_name = strndup(hostname, strlen(hostname));
		gais[h].ar_request = &addr_request;
		h++;
	}
	fclose(file);

	*list = gais;
	return h;
}

static void read_eval_reply(int sock, struct eval_host *evalhosts, int hosts)
{
	struct icmp_packet mypkt;
	mypkt.peer_len = sizeof(struct sockaddr_storage);
	uint8_t buf[BUFSIZ];
	int len = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr *) &mypkt.peer, &mypkt.peer_len);
	if (len > 0) {
		if (icmp_parse(&mypkt, buf, len) == 0) {
			int i;
			for (i = 0; i < hosts; i++) {
				struct eval_host *eh = &evalhosts[i];
				if (memcmp(&mypkt.peer, &eh->host->sockaddr, mypkt.peer_len) == 0 &&
					eh->payload_len == mypkt.payload_len &&
					memcmp(mypkt.payload, eh->payload, eh->payload_len) == 0 &&
					eh->id == mypkt.id &&
					eh->cur_seqno == mypkt.seqno) {

					/* Store accepted reply */
					eh->host->rx_icmp++;
					break;
				}
			}
			free(mypkt.payload);
		}
	}
}

static void evaluate_hosts(struct eval_host *evalhosts, int hosts)
{
	int i;

	printf("Evaluating %d hosts.", hosts);
	for (i = 0; i < 5; i++) {
		int maxfd;
		int h;

		printf(".");
		fflush(stdout);
		for (h = 0; h < hosts; h++) {
			int sock;
			struct icmp_packet pkt;

			memcpy(&pkt.peer, &evalhosts[h].host->sockaddr, evalhosts[h].host->sockaddr_len);
			pkt.peer_len = evalhosts[h].host->sockaddr_len;
			pkt.type = ICMP_REQUEST;
			pkt.id = evalhosts[h].id;
			pkt.seqno = evalhosts[h].cur_seqno;
			pkt.payload = evalhosts[h].payload;
			pkt.payload_len = evalhosts[h].payload_len;

			if (ICMP_ADDRFAMILY(&pkt) == AF_INET) {
				sock = sockv4;
			} else {
				sock = sockv6;
			}

			if (sock > 0) {
				evalhosts[h].host->tx_icmp++;
				icmp_send(sock, &pkt);
			}
		}

		maxfd = MAX(sockv4, sockv6);
		for (;;) {
			struct timeval tv;
			fd_set fds;
			int i;

			FD_ZERO(&fds);
			if (sockv4 > 0) FD_SET(sockv4, &fds);
			if (sockv6 > 0) FD_SET(sockv6, &fds);

			tv.tv_sec = 1;
			tv.tv_usec = 0;
			i = select(maxfd+1, &fds, NULL, NULL, &tv);
			if (!i) break; /* No action for 1 second, break.. */
			if ((sockv4 > 0) && FD_ISSET(sockv4, &fds)) read_eval_reply(sockv4, evalhosts, hosts);
			if ((sockv6 > 0) && FD_ISSET(sockv6, &fds)) read_eval_reply(sockv6, evalhosts, hosts);
		}
	}
	printf(" done.\n");
}

int main(int argc, char **argv)
{
	struct gaicb *gais;
	struct gaicb **list;
	struct eval_host *eval_hosts;
	int hostnames;
	int hosts;
	int addr;
	int i;
	int ret;

	if (argc != 2) {
		fprintf(stderr, "Expected one argument: path of hostname file\n");
		return EXIT_FAILURE;
	}

	hostnames = read_hostnames(argv[1], &gais);
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

	list = calloc(hostnames, sizeof(struct gaicb*));
	for (i = 0; i < hostnames; i++) {
		list[i] = &gais[i];
	}
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

	for (i = 0; i < sizeof(eval_payload); i++) {
		eval_payload[i] = i & 0xff;
	}

	eval_hosts = calloc(hosts, sizeof(struct eval_host));
	addr = 0;
	for (i = 0; i < hostnames; i++) {
		free((char *) list[i]->ar_name);
		if (gai_error(list[i]) == 0) {
			struct addrinfo *result = list[i]->ar_result;
			while (result) {
				struct host *host = malloc(sizeof(struct host));
				bzero(host, sizeof(*host));
				memcpy(&host->sockaddr, result->ai_addr, result->ai_addrlen);
				host->sockaddr_len = result->ai_addrlen;

				eval_hosts[addr].host = host;
				eval_hosts[addr].id = addr;
				eval_hosts[addr].cur_seqno = addr * 2;
				eval_hosts[addr].payload = eval_payload;
				eval_hosts[addr].payload_len = sizeof(eval_payload);
				addr++;

				result = result->ai_next;
			}
			freeaddrinfo(list[i]->ar_result);
		}
	}
	free(gais);
	free(list);

	evaluate_hosts(eval_hosts, hosts);

	int pkt_tx = 0;
	int pkt_rx = 0;
	int tot_ok = 0;
	for (i = 0; i < hosts; i++) {
		pkt_tx += eval_hosts[i].host->tx_icmp;
		pkt_rx += eval_hosts[i].host->rx_icmp;
		if (eval_hosts[i].host->tx_icmp == eval_hosts[i].host->rx_icmp) {
			tot_ok++;
		}
		free(eval_hosts[i].host);
	}
	free(eval_hosts);
	printf("\n%d of %d hosts responded correctly to all pings.\n", tot_ok, hosts);
	printf("In total: %d packets sent, %d packets received.\n", pkt_tx, pkt_rx);

	return EXIT_SUCCESS;
}
