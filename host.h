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

#ifndef PINGFS_HOST_H_
#define PINGFS_HOST_H_

#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <stdlib.h>

struct host {
	struct host *next;
	struct sockaddr_storage sockaddr;
	socklen_t sockaddr_len;
};

int host_make_resolvlist(FILE *hostfile, struct gaicb **list[]);
void host_free_resolvlist(struct gaicb *list[], int length);

struct host *host_create(struct gaicb *list[], int listlength);

int host_evaluate(struct host **hosts, int length, int timeout);

void host_use(struct host* hosts);

struct host *host_get_next();

#endif /* PINGFS_HOST_H_ */
