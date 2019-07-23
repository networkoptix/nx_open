#include "system_allocator.h"

#include <stdlib.h>

void* QnSystemAllocator::alloc(size_t size)
{
    return ::malloc(size);
}

void QnSystemAllocator::release(void* ptr)
{
    ::free(ptr);
}

static QnSystemAllocator QnSystemAllocator_instance;

QnSystemAllocator* QnSystemAllocator::instance()
{
    return &QnSystemAllocator_instance;
}
