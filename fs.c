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

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/param.h>

#define CHUNK_SIZE 1024
struct chunk {
	struct chunk *next;
	struct host *host;
	uint16_t id;
	uint16_t len;
};

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
		next = c->next;
		free(c);
		c = next;
	}
	free((void*) f->name);
	free(f);
}

void fs_cleanup()
{
	struct file *f;

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

static int fs_write(const char *name, const char *buf, size_t size,
	off_t offset, struct fuse_file_info *fileinfo)
{
	struct file *f;
	struct chunk *c;

	f = find_file(name);
	if (!f)
		return -ENOENT;

	/* Only one write supported */
	if (f->chunks)
		return -ENOTSUP;

	c = calloc(1, sizeof(*c));
	c->id = ((uint64_t) f) & 0xFFFF;
	c->len = MIN(size, CHUNK_SIZE);

	f->chunks = c;
	c->host = host_get_next();
	net_send(c->host, c->id, 0, (const uint8_t *) buf, c->len);

	return c->len;
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
};

