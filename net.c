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
#include "net.h"
#include "icmp.h"
#include "chunk.h"

#include <netinet/ip_icmp.h>
#include <netinet/icmp6.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <sys/param.h>
#include <pthread.h>

static int sockv4;
static int sockv6;

struct pkt_stats {
	long long unsigned int packets;
	long long unsigned int bytes;
};

static struct net_data {
	pthread_t responder;
	pthread_t status;
	pthread_mutex_t stats_mutex;
	struct pkt_stats tx;
	struct pkt_stats rx;
} netdata;

static void inc_stats(struct pkt_stats *stats, int packetsize)
{
	pthread_mutex_lock(&netdata.stats_mutex);
	stats->packets++;
	stats->bytes += packetsize + ICMP_HDRLEN;
	pthread_mutex_unlock(&netdata.stats_mutex);
}

static void net_inc_tx(int packetsize)
{
	inc_stats(&netdata.tx, packetsize);
}

void net_inc_rx(int packetsize)
{
	inc_stats(&netdata.rx, packetsize);
}

int net_open_sockets()
{
	/* 1MB receive buffer per socket */
	int rcvbuf = 1024*1024;

	// v4 socket will return full IP header
	sockv4 = socket(PF_INET, SOCK_RAW, IPPROTO_ICMP);
	if (sockv4 < 0) {
		perror("Failed to open IPv4 socket");
	} else {
		int res = setsockopt(sockv4, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));
		if (res < 0) {
			perror("Failed to set receive buffer size on IPv4 socket");
		}
	}

	// v6 socket will just give ICMPv6 data, no IP header
	sockv6 = socket(PF_INET6, SOCK_RAW, IPPROTO_ICMPV6);
	if (sockv6 >= 0) {
		struct icmp6_filter filter;
		int res = setsockopt(sockv6, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));
		if (res < 0) {
			perror("Failed to set receive buffer size on IPv6 socket");
		}

		ICMP6_FILTER_SETBLOCKALL(&filter);
		ICMP6_FILTER_SETPASS(ICMP6_ECHO_REPLY, &filter);
		res = setsockopt(sockv6, IPPROTO_ICMPV6, ICMP6_FILTER, &filter, sizeof(filter));
		if (res < 0) {
			perror("Failed to set ICMP filters on IPv6 socket");
		}
	} else {
		perror("Failed to open IPv6 socket");
	}

	if (sockv4 < 0 && sockv6 < 0)
		return 1;

	return 0;
}

void net_send(struct host *host, uint16_t id, uint16_t seqno, const uint8_t *data, size_t len)
{
	int sock;
	struct icmp_packet pkt;

	memcpy(&pkt.peer, &host->sockaddr, host->sockaddr_len);
	pkt.peer_len = host->sockaddr_len;
	pkt.type = ICMP_REQUEST;
	pkt.id = id;
	pkt.seqno = seqno;
	pkt.payload = (uint8_t *) data;
	pkt.payload_len = len;

	if (ICMP_ADDRFAMILY(&pkt) == AF_INET) {
		sock = sockv4;
	} else {
		sock = sockv6;
	}

	if (sock >= 0) {
		net_inc_tx(pkt.payload_len);
		if (!icmp_send(sock, &pkt))
			perror("Failed sending data packet");
	}

}

static void handle_recv(int sock, net_recv_fn_t recv_fn, void *recv_data)
{
	struct icmp_packet mypkt;
	mypkt.peer_len = sizeof(struct sockaddr_storage);
	uint8_t buf[BUFSIZ];
	int len;

	len = recvfrom(sock, buf, sizeof(buf), 0,
		(struct sockaddr *) &mypkt.peer, &mypkt.peer_len);
	if (len > 0 && icmp_parse(&mypkt, buf, len) == 0) {
		if (mypkt.type == ICMP_REPLY) {
			recv_fn(recv_data, &mypkt.peer, mypkt.peer_len, mypkt.id,
				mypkt.seqno, &mypkt.payload, mypkt.payload_len);
		}
		free(mypkt.payload);
	}
}

int net_recv(struct timeval *tv, net_recv_fn_t recv_fn, void *recv_data)
{
	int maxfd;
	fd_set fds;
	int i;

	FD_ZERO(&fds);
	if (sockv4 >= 0) FD_SET(sockv4, &fds);
	if (sockv6 >= 0) FD_SET(sockv6, &fds);
	maxfd = MAX(sockv4, sockv6);

	i = select(maxfd+1, &fds, NULL, NULL, tv);
	if ((sockv4 >= 0) && FD_ISSET(sockv4, &fds))
		handle_recv(sockv4, recv_fn, recv_data);
	if ((sockv6 >= 0) && FD_ISSET(sockv6, &fds))
		handle_recv(sockv6, recv_fn, recv_data);
	return i;
}

static void *responder_thread(void *arg)
{
	for (;;) {
		struct timeval tv;
		tv.tv_sec = 1;
		tv.tv_usec = 0;
		net_recv(&tv, chunk_reply, NULL);
	}
	return NULL;
}

static void get_stats(struct pkt_stats *rx, struct pkt_stats *tx)
{
	pthread_mutex_lock(&netdata.stats_mutex);
	memcpy(rx, &netdata.rx, sizeof(netdata.rx));
	memcpy(tx, &netdata.tx, sizeof(netdata.tx));
	pthread_mutex_unlock(&netdata.stats_mutex);
}

static float format_bytes(unsigned long long bytes, const char **suffix)
{
	const char *suffixes[] = {
		"B",
		"kB",
		"MB",
		"GB",
		NULL,
	};
	int i = 0;
	float bps = (float) bytes;

	while (suffixes[i + 1] && bps > 1300) {
		bps /= 1000.0f;
		i++;
	}

	*suffix = suffixes[i];

	return bps;
}

static void diff_stats(struct pkt_stats *new, struct pkt_stats *old)
{
	struct pkt_stats diff;
	float bytes;
	const char *byte_suffix;
	diff.packets = new->packets - old->packets;
	diff.bytes = new->bytes - old->bytes;
	memcpy(old, new, sizeof(*old));
	bytes = format_bytes(diff.bytes, &byte_suffix);
	printf("%6llu pkt/s, %7.01f %2s/s", diff.packets, bytes, byte_suffix);
}

static void *status_thread(void *arg)
{
	const struct timespec status_sleep = {
		.tv_sec = 1,
		.tv_nsec = 0,
	};
	static struct pkt_stats prev_rx, prev_tx;
	get_stats(&prev_rx, &prev_tx);
	nanosleep(&status_sleep, NULL);
	for (;;) {
		struct pkt_stats rx, tx;
		get_stats(&rx, &tx);
		printf("\rICMP in: ");
		diff_stats(&rx, &prev_rx);
		printf("    ICMP out: ");
		diff_stats(&tx, &prev_tx);
		fflush(stdout);
		nanosleep(&status_sleep, NULL);
	}
	return NULL;
}

void net_start()
{
	if (pthread_mutex_init(&netdata.stats_mutex, NULL)) {
		perror("Fatal, failed to create a mutex");
		exit(EXIT_FAILURE);
	}

	pthread_create(&netdata.responder, NULL, responder_thread, NULL);
	pthread_create(&netdata.status, NULL, status_thread, NULL);
}

void net_stop()
{
	pthread_cancel(netdata.responder);
	pthread_join(netdata.responder, NULL);
	pthread_cancel(netdata.status);
	pthread_join(netdata.status, NULL);

	pthread_mutex_lock(&netdata.stats_mutex);
	printf("\n\nTotal network resources consumed:\n"
		"in:  %10llu packets, %10llu bytes\n"
		"out: %10llu packets, %10llu bytes\n"
		" (bytes counted above IP level)\n",
		netdata.rx.packets, netdata.rx.bytes,
		netdata.tx.packets, netdata.tx.bytes
	);
	pthread_mutex_unlock(&netdata.stats_mutex);
}
