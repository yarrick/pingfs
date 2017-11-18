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

#ifndef PINGFS_NET_H_
#define PINGFS_NET_H_

#include "host.h"

#include <stdint.h>
#include <sys/types.h>

int net_open_sockets();
void net_send(struct host *host, uint16_t id, uint16_t seqno, const uint8_t *data, size_t len);

typedef void (*net_recv_fn_t)(void *userdata, struct sockaddr_storage *addr,
	size_t addrlen, uint16_t id, uint16_t seqno, uint8_t **data, size_t len);

int net_recv(struct timeval *tv, net_recv_fn_t recv_fn, void *recv_data);

void net_inc_rx(int packetsize);

void net_start();
void net_stop();

#endif /* PINGFS_NET_H_ */
