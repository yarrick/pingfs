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

int net_open_sockets()
{
	// v4 socket will return full IP header
	sockv4 = socket(PF_INET, SOCK_RAW, IPPROTO_ICMP);
	if (sockv4 < 0) {
		perror("Failed to open IPv4 socket");
	}

	// v6 socket will just give ICMPv6 data, no IP header
	sockv6 = socket(PF_INET6, SOCK_RAW, IPPROTO_ICMPV6);
	if (sockv6 >= 0) {
		struct icmp6_filter filter;
		ICMP6_FILTER_SETBLOCKALL(&filter);
		ICMP6_FILTER_SETPASS(ICMP6_ECHO_REPLY, &filter);
		setsockopt(sockv6, IPPROTO_ICMPV6, ICMP6_FILTER, &filter, sizeof(filter));
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
		host->tx_icmp++;
		icmp_send(sock, &pkt);
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

void *net_start_responder()
{
	pthread_t *thread = malloc(sizeof(pthread_t));
	pthread_create(thread, NULL, responder_thread, NULL);
	return thread;
}

void net_stop_responder(void *data)
{
	pthread_t *thread = (pthread_t *) data;
	pthread_cancel(*thread);
	pthread_join(*thread, NULL);
	free(thread);
}
