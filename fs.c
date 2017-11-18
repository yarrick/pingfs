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
#include "fs.h"
#include "host.h"
#include "net.h"
#include "chunk.h"

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/param.h>
#include <assert.h>

struct file {
	struct file *next;
	const char *name;
	struct chunk *chunks;
	mode_t mode;
};

struct file *files;

static void fs_free(struct file *f)
{
	struct chunk *c, *next;

	c = f->chunks;
	while (c) {
		next = c->next_file;
		chunk_remove(c);
		chunk_free(c);
		c = next;
	}
	free((void*) f->name);
	free(f);
}

static size_t file_size(struct file *f)
{
	struct chunk *c;
	size_t size = 0;

	c = f->chunks;
	while (c) {
		size += c->len;
		c = c->next_file;
	}
	return size;
}

static void *fs_init(struct fuse_conn_info *conn)
{
	net_start();
	return NULL;
}

static void fs_destroy(void *data)
{
	struct file *f;

	net_stop();
	f = files;
	while (f) {
		struct file *next = f->next;
		fs_free(f);
		f = next;
	}
}

static struct file *find_file(const char *name)
{
	struct file *f;

	f = files;
	while (f) {
		if (strcmp(name, f->name) == 0)
			return f;
		f = f->next;
	}
	return NULL;
}

static int fs_mkdir(const char *name, mode_t mode)
{
	return -ENOTSUP;
}

static int fs_mknod(const char *name, mode_t mode, dev_t device)
{
	struct file *f;

	/* Only regular files */
	if (!S_ISREG(mode))
		return -ENOTSUP;

	f = find_file(name);
	if (f)
		return -EEXIST;

	f = calloc(1, sizeof(struct file));
	if (!f)
		return -errno;

	f->name = strdup(name);
	f->mode = mode;
	f->next = files;
	files = f;

	return 0;
}

static int fs_chmod(const char *name, mode_t mode)
{
	struct file *f;

	f = find_file(name);
	if (!f)
		return -ENOENT;

	f->mode = mode;
	return 0;
}

static int fs_utime(const char *name, struct utimbuf *utim)
{
	struct file *f;

	f = find_file(name);
	if (!f)
		return -ENOENT;

	/* No-op */
	return 0;
}

static int fs_getattr(const char *name, struct stat *stat)
{
	struct file *f;

	stat->st_nlink = 1;
	stat->st_uid = getuid();
	stat->st_gid = getgid();
	stat->st_atime = 0;
	stat->st_mtime = 0;
	stat->st_ctime = 0;

	if (strcmp("/", name) == 0) {
		stat->st_mode = S_IFDIR | 0775;
		stat->st_size = 0;
		stat->st_blksize = 0;
		stat->st_blocks = 0;
		return 0;
	}

	f = find_file(name);
	if (!f)
		return -ENOENT;

	stat->st_mode = f->mode;
	stat->st_size = file_size(f);
	return 0;
}

static int fs_unlink(const char *name)
{
	struct file *f = files;
	struct file *last = NULL;
	while (f) {
		if (strcmp(name, f->name) == 0) {
			if (last) {
				last->next = f->next;
			} else {
				files = f->next;
			}
			fs_free(f);
			return 0;
		}
		last = f;
		f = f->next;
	}
	return -ENOENT;
}

static int fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
	off_t offset, struct fuse_file_info *fi)
{
	struct file *f;
	if (strcmp("/", path)) {
		return -ENOENT;
	}
	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);

	f = files;
	while (f) {
		/* Skip initial '/' in name */
		filler(buf, &f->name[1], NULL, 0);
		f = f->next;
	}

	return 0;
}

static int fs_open(const char *name, struct fuse_file_info *fileinfo)
{
	struct file *f;

	f = find_file(name);
	if (!f)
		return -ENOENT;

	return 0;
}

static int fs_inner_write(struct file *f, const char *buf, size_t size,
	off_t offset)
{
	struct chunk *c;
	struct chunk *last = NULL;
	uint8_t *chunkdata;
	int len;
	int clen;

	c = f->chunks;
	while (c && offset >= c->len) {
		if (c->len != CHUNK_SIZE) {
			/* Extend this chunk instead
			 * of creating new */
			break;
		}
		offset -= c->len;
		last = c;
		c = c->next_file;
	}
	if (!c) {
		/* Write to new chunk */
		c = chunk_create();
		c->len = MIN(size, CHUNK_SIZE);
		chunk_add(c);

		if (last)
			last->next_file = c;
		else
			f->chunks = c;
		c->host = host_get_next();
		net_send(c->host, c->id, c->seqno, (const uint8_t *) buf, c->len);

		return c->len;
	}
	/* Modify/extend existing chunk */

	chunkdata = NULL;
	clen = chunk_wait_for(c, &chunkdata);
	if (clen <= 0)
		return clen;

	/* New chunk length */
	clen = MIN(CHUNK_SIZE, size + offset);
	/* Number of bytes to write */
	len = MIN(clen - offset, size);

	chunkdata = realloc(chunkdata, clen);
	memcpy(&chunkdata[offset], buf, len);
	chunk_done(c, chunkdata, clen);
	return len;
}

static int fs_write(const char *name, const char *buf, size_t size,
	off_t offset, struct fuse_file_info *fileinfo)
{
	struct file *f;

	f = find_file(name);
	if (!f)
		return -ENOENT;

	return fs_inner_write(f, buf, size, offset);
}

static int fs_read(const char *name, char *buf, size_t size,
	off_t offset, struct fuse_file_info *fileinfo)
{
	struct file *f;
	struct chunk *c;
	uint8_t *chunkdata;
	int len;
	int clen;

	f = find_file(name);
	if (!f)
		return -ENOENT;

	c = f->chunks;
	while (c && offset >= c->len) {
		offset -= c->len;
		c = c->next_file;
	}
	if (!c) {
		/* Read out of bounds */
		return 0;
	}

	len = MIN(c->len - offset, size);

	chunkdata = NULL;
	clen = chunk_wait_for(c, &chunkdata);
	if (!clen)
		return -EIO;

	memcpy(buf, &chunkdata[offset], len);
	chunk_done(c, chunkdata, clen);
	return len;
}

static int shrink_file(struct file *f, off_t length)
{
	struct chunk *c = f->chunks;
	struct chunk *prev = NULL;
	while (c->len <= length) {
		length -= c->len;
		prev = c;
		c = c->next_file;
	}
	if (!length) {
		if (prev)
			prev->next_file = NULL;
		else
			f->chunks = NULL;
	}
	while (c) {
		struct chunk *next = c->next_file;
		if (length) {
			uint8_t *cdata;
			int clen;

			clen = chunk_wait_for(c, &cdata);
			if (!clen)
				return -EIO;

			chunk_done(c, cdata, length);
			c->next_file = NULL;
			length = 0;
		} else {
			chunk_remove(c);
			chunk_free(c);
		}
		c = next;
	}
	return 0;
}

static int grow_file(struct file *f, off_t length)
{
	int offset = file_size(f);
	int to_grow = length - offset;
	char zerobuf[CHUNK_SIZE];

	memset(zerobuf, 0, sizeof(zerobuf));
	while (to_grow) {
		int res = fs_inner_write(f, zerobuf, MIN(to_grow, CHUNK_SIZE), offset);
		if (res < 0) {
			return res;
		}
		assert(res);
		offset += res;
		to_grow -= res;
	}
	return 0;
}

static int fs_truncate(const char *name, off_t length)
{
	struct file *f;
	int cur_size;

	f = find_file(name);
	if (!f)
		return -ENOENT;

	cur_size = file_size(f);
	if (length > cur_size)
		return grow_file(f, length);
	if (length < cur_size) {
		return shrink_file(f, length);
	}
	return 0;
}

static int fs_rename(const char *name, const char *newname)
{
	struct file *f;

	f = find_file(name);
	if (!f)
		return -ENOENT;

	free((void*) f->name);
	f->name = strdup(newname);

	return 0;
}

const struct fuse_operations fs_ops = {
	.getattr = fs_getattr,
	.utime = fs_utime,
	.chmod = fs_chmod,
	.mkdir = fs_mkdir,
	.mknod = fs_mknod,
	.unlink = fs_unlink,
	.readdir = fs_readdir,
	.open = fs_open,
	.write = fs_write,
	.read = fs_read,
	.truncate = fs_truncate,
	.rename = fs_rename,
	.init = fs_init,
	.destroy = fs_destroy,
};

