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

#ifndef CEDRUS_MEM_H_
#define CEDRUS_MEM_H_

#include <stddef.h>
#include <stdint.h>
#include "kernel-headers/cedardev_api.h"

struct cedrus_mem
{
	void *virt;
	uint32_t phys;
	size_t size;
};

struct cedrus_allocator
{
	struct cedrus_mem *(*mem_alloc)(struct cedrus_allocator *allocator, size_t size);
	void (*mem_free)(struct cedrus_allocator *allocator, struct cedrus_mem *mem);
	void (*mem_flush)(struct cedrus_allocator *allocator, struct cedrus_mem *mem);

	void (*free)(struct cedrus_allocator *allocator);
};

struct cedrus_allocator *cedrus_allocator_ve_new(int ve_fd, const struct cedarv_env_infomation *ve_info);
struct cedrus_allocator *cedrus_allocator_ion_new(void);
#ifdef USE_UMP
struct cedrus_allocator *cedrus_allocator_ump_new(void);
#endif

// ATTENTION: Defined in cedrus.c.
uint32_t phys2bus(uint32_t phys);
uint32_t bus2phys(uint32_t bus);

#endif
