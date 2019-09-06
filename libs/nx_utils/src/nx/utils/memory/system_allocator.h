#pragma once

#include "abstract_allocator.h"

/**
 * Allocates memory using \a malloc and frees with \a free.
 */
class NX_UTILS_API QnSystemAllocator:
    public QnAbstractAllocator
{
public:
    virtual void* alloc(size_t size) override;
    virtual void release(void* ptr) override;

    static QnSystemAllocator* instance();
};
