#pragma once

#include <stdlib.h>

/**
 * Using abstract class for allocator to leave QnAbstractMediaData and children largely unchanged.
 */
class NX_UTILS_API QnAbstractAllocator
{
public:
    virtual ~QnAbstractAllocator() = default;

    virtual void* alloc(size_t size) = 0;
    virtual void release(void* ptr) = 0;
};
