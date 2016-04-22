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

#ifndef CEDRUS_H_
#define CEDRUS_H_

#include <stddef.h>
#include <stdint.h>

typedef struct cedrus cedrus_t;

enum cedrus_engine { CEDRUS_ENGINE_MPEG = 0x0, CEDRUS_ENGINE_H264 = 0x1, CEDRUS_ENGINE_HEVC = 0x4 };

cedrus_t *cedrus_open(void);
void cedrus_close(cedrus_t *dev);
int cedrus_get_ve_version(cedrus_t *dev);
int cedrus_ve_wait(cedrus_t *dev, int timeout);
void *cedrus_ve_get(cedrus_t *dev, enum cedrus_engine engine, uint32_t flags);
void cedrus_ve_put(cedrus_t *dev);

typedef struct cedrus_mem cedrus_mem_t;

cedrus_mem_t *cedrus_mem_alloc(cedrus_t *dev, size_t size);
void cedrus_mem_free(cedrus_mem_t *mem);
void cedrus_mem_flush_cache(cedrus_mem_t *mem);
void *cedrus_mem_get_pointer(const cedrus_mem_t *mem);
uint32_t cedrus_mem_get_phys_addr(const cedrus_mem_t *mem);
uint32_t cedrus_mem_get_bus_addr(const cedrus_mem_t *mem);

#endif
