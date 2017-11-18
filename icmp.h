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

#ifndef PINGFS_ICMP_H_
#define PINGFS_ICMP_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define ICMP_ADDRFAMILY(pkt) ((pkt)->peer.ss_family)

#define ICMP_HDRLEN 8

enum icmp_type {
	ICMP_REQUEST,
	ICMP_REPLY,
};

struct icmp_packet {
	struct sockaddr_storage peer;
	socklen_t peer_len;
	enum icmp_type type;
	uint16_t id;
	uint16_t seqno;
	uint8_t *payload;
	uint32_t payload_len;
};

extern int icmp_parse(struct icmp_packet *pkt, uint8_t *data, int len);
extern void icmp_dump(struct icmp_packet *pkt);
extern int icmp_send(int socket, struct icmp_packet *pkt);

#endif /* PINGFS_ICMP_H_ */
