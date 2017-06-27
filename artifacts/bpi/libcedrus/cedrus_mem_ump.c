/*
 * Copyright (c) 2016 Jens Kuske <jenskuske@gmail.com>
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
#include <ump/ump.h>
#include <ump/ump_ref_drv.h>
#include "cedrus_mem.h"

struct ump_mem
{
	struct cedrus_mem pub;

	ump_handle handle;
};

static struct cedrus_mem *cedrus_allocator_ump_mem_alloc(struct cedrus_allocator *allocator, size_t size)
{
	(void)allocator;
	struct ump_mem *mem = calloc(1, sizeof(*mem));
	if (!mem)
		return NULL;

	mem->pub.size = size;

	mem->handle = ump_ref_drv_allocate(size, UMP_REF_DRV_CONSTRAINT_PHYSICALLY_LINEAR);
	if (mem->handle == UMP_INVALID_MEMORY_HANDLE)
		goto err;

	mem->pub.virt = ump_mapped_pointer_get(mem->handle);
	if (!mem->pub.virt)
		goto err_release;

	mem->pub.phys = bus2phys((uint32_t)ump_phys_address_get(mem->handle));
	if (!mem->pub.phys)
		goto err_release;

	return &mem->pub;

err_release:
	ump_reference_release(mem->handle);
err:
	free(mem);
	return NULL;
}

static void cedrus_allocator_ump_mem_free(struct cedrus_allocator *allocator, struct cedrus_mem *mem_pub)
{
	(void)allocator;
	struct ump_mem *mem = (struct ump_mem *)mem_pub;

	ump_reference_release(mem->handle);

	free(mem);
}

static void cedrus_allocator_ump_mem_flush(struct cedrus_allocator *allocator, struct cedrus_mem *mem_pub)
{
	(void)allocator;
	struct ump_mem *mem = (struct ump_mem *)mem_pub;

	ump_cpu_msync_now(mem->handle, UMP_MSYNC_CLEAN_AND_INVALIDATE, 0, mem->pub.size);
}

static void cedrus_allocator_ump_free(struct cedrus_allocator *allocator)
{
	ump_close();
	free(allocator);
}

struct cedrus_allocator *cedrus_allocator_ump_new(void)
{
	struct cedrus_allocator *allocator = calloc(1, sizeof(*allocator));
	if (!allocator)
		return NULL;

	if (ump_open() != UMP_OK)
	{
		free(allocator);
		return NULL;
	}

	allocator->mem_alloc = cedrus_allocator_ump_mem_alloc;
	allocator->mem_free = cedrus_allocator_ump_mem_free;
	allocator->mem_flush = cedrus_allocator_ump_mem_flush;

	allocator->free = cedrus_allocator_ump_free;

	return allocator;
}
