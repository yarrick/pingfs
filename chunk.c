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

#include <pthread.h>

static uint16_t id;

static struct chunk *chunk_head;
static pthread_mutex_t chunk_mutex = PTHREAD_MUTEX_INITIALIZER;

struct chunk *chunk_create()
{
	struct chunk *c;

	c = calloc(1, sizeof(*c));

	/* TODO can give duplicate ids after some time
	 * if objects have varying lifetime */
	c->id = id++;

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
