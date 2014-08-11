#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <stdlib.h>

struct host {
	unsigned long tx_icmp;
	unsigned long rx_icmp;
	struct sockaddr_storage sockaddr;
	socklen_t sockaddr_len;
};

struct eval_host {
	struct host *host;
	uint16_t cur_seqno;
	uint16_t id;
	uint8_t *payload;
	size_t payload_len;
};

int host_make_resolvlist(FILE *hostfile, struct gaicb **list[]);
void host_free_resolvlist(struct gaicb *list[], int length);

struct host *host_create(struct gaicb *list[], int listlength, int hostlength);

int host_evaluate(struct host *hostlist, int length, int sockv4, int sockv6);
