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
#include "chunk.h"
#include "host.h"
#include "net.h"

#include <time.h>
#include <pthread.h>
#include <errno.h>

enum io_owner {
	OWNER_FS = 1,
	OWNER_NET = 2,
};

struct io {
	pthread_cond_t fs_cond;
	pthread_cond_t net_cond;
	pthread_mutex_t mutex;
	enum io_owner owner;
	uint8_t *data;
	size_t len;
};

static uint16_t icmp_id;
static int timeout;

static struct chunk *chunk_head;
static pthread_mutex_t chunk_mutex = PTHREAD_MUTEX_INITIALIZER;

void chunk_set_timeout(int t)
{
	timeout = t;
}

struct chunk *chunk_create()
{
	struct chunk *c;

	c = calloc(1, sizeof(*c));
	if (!c)
		return NULL;

	/* TODO can give duplicate ids after some time
	 * if objects have varying lifetime */
	c->id = icmp_id++;

	return c;
}

void chunk_free(struct chunk *c)
{
	free(c);
}

void chunk_add(struct chunk *c)
{
	pthread_mutex_lock(&chunk_mutex);
	c->next_active = chunk_head;
	chunk_head = c;
	pthread_mutex_unlock(&chunk_mutex);
}

void chunk_remove(struct chunk *c)
{
	struct chunk *prev, *curr;

	prev = NULL;
	pthread_mutex_lock(&chunk_mutex);
	curr = chunk_head;
	while (curr) {
		if (curr == c) {
			if (prev) {
				prev->next_active = curr->next_active;
			} else {
				chunk_head = curr->next_active;
			}
			break;
		}
		prev = curr;
		curr = curr->next_active;
	}
	pthread_mutex_unlock(&chunk_mutex);
}

static void process_chunk(struct chunk *c, uint8_t **data)
{
	c->seqno++;
	if (c->io) {
		struct io *io = c->io;
		pthread_mutex_lock(&io->mutex);
		io->data = *data;
		io->len = c->len;
		io->owner = OWNER_FS;
		pthread_cond_signal(&io->fs_cond);
		/* Wait while fs thread works, sets owner back and signals */
		while (io->owner != OWNER_NET)
			pthread_cond_wait(&io->net_cond, &io->mutex);
		*data = io->data;
		pthread_mutex_unlock(&io->mutex);
		free(c->io);
		c->io = NULL;
	}
	net_send(c->host, c->id, c->seqno, *data, c->len);
}

void chunk_reply(void *userdata, struct sockaddr_storage *addr,
	size_t addrlen, uint16_t id, uint16_t seqno, uint8_t **data, size_t len)
{
	struct chunk *c;
	pthread_mutex_lock(&chunk_mutex);
	c = chunk_head;
	while (c) {
		if (c->id == id) {
			net_inc_rx(len);
			if (len == c->len && seqno == c->seqno) {
				process_chunk(c, data);
			}
			break;
		}
		c = c->next_active;
	}
	pthread_mutex_unlock(&chunk_mutex);
}

/* Call from fs thread to wait until chunk arrives or timeout.
 * When data comes from icmp it is pointed to in data argument,
 * and the function returns the length of it.
 * Must call chunk_done() after when done */
int chunk_wait_for(struct chunk *c, uint8_t **data)
{
	pthread_mutex_lock(&chunk_mutex);
	if (c->io) {
		pthread_mutex_unlock(&chunk_mutex);
		return -EBUSY;
	}

	c->io = calloc(1, sizeof(struct io));
	if (!c->io) {
		pthread_mutex_unlock(&chunk_mutex);
		return -ENOMEM;
	}

	pthread_mutex_unlock(&chunk_mutex);
	c->io->owner = OWNER_NET;
	pthread_cond_init(&c->io->fs_cond, NULL);
	pthread_cond_init(&c->io->net_cond, NULL);
	if (pthread_mutex_init(&c->io->mutex, NULL)) {
		pthread_mutex_unlock(&chunk_mutex);
		free(c->io);
		return -errno;
	}

	pthread_mutex_lock(&c->io->mutex);
	while (c->io->owner != OWNER_FS) {
		int res;
		struct timespec ts;
		clock_gettime(CLOCK_REALTIME, &ts);
		ts.tv_sec += timeout;
		res = pthread_cond_timedwait(&c->io->fs_cond,
			&c->io->mutex, &ts);
		if (res) {
			/* Timeout, data is lost */
			pthread_mutex_unlock(&c->io->mutex);
			pthread_mutex_lock(&chunk_mutex);
			free(c->io);
			c->io = NULL;
			pthread_mutex_unlock(&chunk_mutex);
			return 0;
		}
	}

	/* Still holding io->mutex here */
	*data = c->io->data;
	return c->io->len;
}

/* Put back new data, let net thread continue */
void chunk_done(struct chunk *c, uint8_t *data, size_t len)
{
	c->io->data = data;
	c->io->len = len;
	c->len = c->io->len;
	c->io->owner = OWNER_NET;

	pthread_cond_signal(&c->io->net_cond);
	pthread_mutex_unlock(&c->io->mutex);
}
