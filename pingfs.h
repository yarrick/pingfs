#ifndef __PINGFS_H__
#define __PINGFS_H__

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

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

#endif
