/*
 * Copyright (c) 2015-2016 Jens Kuske <jenskuske@gmail.com>
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

#include <fcntl.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include "cedrus_mem.h"
#include "kernel-headers/ion.h"
#include "kernel-headers/ion_sunxi.h"

struct ion_mem
{
	struct cedrus_mem pub;

	ion_user_handle_t handle;
	int fd;
};

struct ion_allocator
{
	struct cedrus_allocator pub;

	int fd;
};

static ion_user_handle_t ion_alloc(int ion_fd, size_t size)
{
	struct ion_allocation_data allocation_data = {
		.len = size,
		.align = 4096,
		.heap_id_mask = ION_HEAP_TYPE_DMA_MASK,
		.flags = ION_FLAG_CACHED | ION_FLAG_CACHED_NEEDS_SYNC,
	};

	if (ioctl(ion_fd, ION_IOC_ALLOC, &allocation_data))
		return 0;

	return allocation_data.handle;
}

static int ion_map(int ion_fd, ion_user_handle_t ion_handle)
{
	struct ion_fd_data fd_data = {
		.handle = ion_handle,
	};

	if (ioctl(ion_fd, ION_IOC_MAP, &fd_data))
		return -1;

	return fd_data.fd;
}

static uint32_t ion_get_phys_addr(int ion_fd, ion_user_handle_t ion_handle)
{
	sunxi_phys_data phys_data = {
		.handle = ion_handle,
	};

	struct ion_custom_data custom_data = {
		.cmd = ION_IOC_SUNXI_PHYS_ADDR,
		.arg = (unsigned long)(&phys_data),
	};

	if (ioctl(ion_fd, ION_IOC_CUSTOM, &custom_data))
		return 0x0;

	return phys_data.phys_addr;
}

static int ion_flush_cache(int ion_fd, void *addr, size_t size)
{
	static int new_ioctl = 0;

	sunxi_cache_range cache_range = {
		.start = (long)addr,
		.end = (long)addr + size,
	};

	struct ion_custom_data custom_data = {
		.cmd = ION_IOC_SUNXI_FLUSH_RANGE,
		.arg = (unsigned long)(&cache_range),
	};

	if (new_ioctl || ioctl(ion_fd, ION_IOC_CUSTOM, &custom_data))
	{
		if (ioctl(ion_fd, ION_IOC_SUNXI_FLUSH_RANGE, &cache_range))
			return 0;
		else
			new_ioctl = 1;
	}

	return 1;
}

static int ion_free(int ion_fd, ion_user_handle_t ion_handle)
{
	struct ion_handle_data handle_data = {
		.handle = ion_handle,
	};

	if (ioctl(ion_fd, ION_IOC_FREE, &handle_data))
		return 0;

	return 1;
}

static struct cedrus_mem *cedrus_allocator_ion_mem_alloc(struct cedrus_allocator *allocator_pub, size_t size)
{
	struct ion_allocator *allocator = (struct ion_allocator *)allocator_pub;

	struct ion_mem *mem = calloc(1, sizeof(*mem));
	if (!mem)
		return NULL;

	mem->pub.size = size;

	mem->handle = ion_alloc(allocator->fd, size);
	if (!mem->handle)
		goto err;

	mem->pub.phys = ion_get_phys_addr(allocator->fd, mem->handle);
	if (!mem->pub.phys)
		goto err_free;

	mem->fd = ion_map(allocator->fd, mem->handle);
	if (mem->fd < 0)
		goto err_free;

	mem->pub.virt = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, mem->fd, 0);
	if (mem->pub.virt == MAP_FAILED)
		goto err_close;

	return &mem->pub;

err_close:
	close(mem->fd);
err_free:
	ion_free(allocator->fd, mem->handle);
err:
	free(mem);
	return NULL;
}

static void cedrus_allocator_ion_mem_free(struct cedrus_allocator *allocator_pub, struct cedrus_mem *mem_pub)
{
	struct ion_allocator *allocator = (struct ion_allocator *)allocator_pub;
	struct ion_mem *mem = (struct ion_mem *)mem_pub;

	munmap(mem->pub.virt, mem->pub.size);
	close(mem->fd);
	ion_free(allocator->fd, mem->handle);

	free(mem);
}

static void cedrus_allocator_ion_mem_flush(struct cedrus_allocator *allocator_pub, struct cedrus_mem *mem_pub)
{
	struct ion_allocator *allocator = (struct ion_allocator *)allocator_pub;
	struct ion_mem *mem = (struct ion_mem *)mem_pub;

	ion_flush_cache(allocator->fd, mem->pub.virt, mem->pub.size);
}

static void cedrus_allocator_ion_free(struct cedrus_allocator *allocator_pub)
{
	struct ion_allocator *allocator = (struct ion_allocator *)allocator_pub;

	close(allocator->fd);
	free(allocator);
}

struct cedrus_allocator *cedrus_allocator_ion_new(void)
{
	struct ion_allocator *allocator = calloc(1, sizeof(*allocator));
	if (!allocator)
		return NULL;

	allocator->fd = open("/dev/ion", O_RDONLY);
	if (allocator->fd == -1)
	{
		free(allocator);
		return NULL;
	}

	allocator->pub.mem_alloc = cedrus_allocator_ion_mem_alloc;
	allocator->pub.mem_free = cedrus_allocator_ion_mem_free;
	allocator->pub.mem_flush = cedrus_allocator_ion_mem_flush;

	allocator->pub.free = cedrus_allocator_ion_free;

	return &allocator->pub;
}
