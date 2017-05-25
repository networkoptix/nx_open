/*
 * Copyright (c) 2013-2016 Jens Kuske <jenskuske@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <pthread.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include "cedrus_mem.h"
#include "kernel-headers/cedardev_api.h"

#define PAGE_OFFSET (0xc0000000) // from kernel

struct ve_mem
{
	struct cedrus_mem pub;

	struct ve_mem *next;
};

struct ve_allocator
{
	struct cedrus_allocator pub;

	int fd;
	struct ve_mem first;
	pthread_mutex_t lock;
};

static struct cedrus_mem *cedrus_allocator_ve_mem_alloc(struct cedrus_allocator *allocator_pub, size_t size)
{
	struct ve_allocator *allocator = (struct ve_allocator *)allocator_pub;

	if (pthread_mutex_lock(&allocator->lock))
		return NULL;

	void *addr = NULL;
	struct cedrus_mem *ret = NULL;

	struct ve_mem *c, *best_chunk = NULL;
	for (c = &allocator->first; c != NULL; c = c->next)
	{
		if(c->pub.virt == NULL && c->pub.size >= size)
		{
			if (best_chunk == NULL || c->pub.size < best_chunk->pub.size)
				best_chunk = c;

			if (c->pub.size == size)
				break;
		}
	}

	if (!best_chunk)
		goto out;

	int left_size = best_chunk->pub.size - size;

	addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, allocator->fd, phys2bus(best_chunk->pub.phys) + PAGE_OFFSET);
	if (addr == MAP_FAILED)
	{
		ret = NULL;
		goto out;
	}

	best_chunk->pub.virt = addr;
	best_chunk->pub.size = size;

	ret = &best_chunk->pub;

	if (left_size > 0)
	{
		c = calloc(1, sizeof(*c));
		if (!c)
			goto out;

		c->pub.phys = best_chunk->pub.phys + size;
		c->pub.size = left_size;
		c->pub.virt = NULL;
		c->next = best_chunk->next;
		best_chunk->next = c;
	}

out:
	pthread_mutex_unlock(&allocator->lock);

	return ret;
}

static void cedrus_allocator_ve_mem_free(struct cedrus_allocator *allocator_pub, struct cedrus_mem *mem_pub)
{
	struct ve_allocator *allocator = (struct ve_allocator *)allocator_pub;
	struct ve_mem *mem = (struct ve_mem *)mem_pub;

	if (pthread_mutex_lock(&allocator->lock))
		return;

	struct ve_mem *c;
	for (c = &allocator->first; c != NULL; c = c->next)
	{
		if (&c->pub == &mem->pub)
		{
			munmap(c->pub.virt, c->pub.size);
			c->pub.virt = NULL;
			break;
		}
	}

	for (c = &allocator->first; c != NULL; c = c->next)
	{
		if (c->pub.virt == NULL)
		{
			while (c->next != NULL && c->next->pub.virt == NULL)
			{
				struct ve_mem *n = c->next;
				c->pub.size += n->pub.size;
				c->next = n->next;
				free(n);
			}
		}
	}

	pthread_mutex_unlock(&allocator->lock);
}

static void cedrus_allocator_ve_mem_flush(struct cedrus_allocator *allocator_pub, struct cedrus_mem *mem_pub)
{
	struct ve_allocator *allocator = (struct ve_allocator *)allocator_pub;
	struct ve_mem *mem = (struct ve_mem *)mem_pub;

	struct cedarv_cache_range cache_range = {
		.start = (long)mem->pub.virt,
		.end = (long)mem->pub.virt + mem->pub.size
	};

	ioctl(allocator->fd, IOCTL_FLUSH_CACHE, &cache_range);
}

static void cedrus_allocator_ve_free(struct cedrus_allocator *allocator_pub)
{
	struct ve_allocator *allocator = (struct ve_allocator *)allocator_pub;

	free(allocator);
}

struct cedrus_allocator *cedrus_allocator_ve_new(int ve_fd, const struct cedarv_env_infomation *ve_info)
{
	if (ve_info->phymem_total_size == 0)
		return NULL;

	struct ve_allocator *allocator = calloc(1, sizeof(*allocator));
	if (!allocator)
		return NULL;

	allocator->fd = ve_fd;

	allocator->first.pub.phys = bus2phys(ve_info->phymem_start - PAGE_OFFSET);
	allocator->first.pub.size = ve_info->phymem_total_size;

	pthread_mutex_init(&allocator->lock, NULL);

	allocator->pub.mem_alloc = cedrus_allocator_ve_mem_alloc;
	allocator->pub.mem_free = cedrus_allocator_ve_mem_free;
	allocator->pub.mem_flush = cedrus_allocator_ve_mem_flush;

	allocator->pub.free = cedrus_allocator_ve_free;

	return &allocator->pub;
}
