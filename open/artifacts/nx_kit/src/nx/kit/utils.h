// Copyright 2018-present Network Optix, Inc.
#pragma once

/**@file
 * Variuos utilities.
 *
 * This unit can be compiled in the context of any C++ project.
 */

#include <cstring>
#include <cstdlib>
#include <memory>

#if !defined(NX_KIT_API)
    #define NX_KIT_API /*empty*/
#endif

namespace nx {
namespace kit {
namespace utils {

/** Alignes val up to alignment boundary. */
inline size_t alignUp(size_t val, size_t alignment)
{
    const size_t remainder = val % alignment;
    if (remainder == 0)
        return val;
    return val + (alignment - remainder);
}

/**
 * Allocate size bytes of data, aligned to alignment boundary.
 *
 * NOTE: Allocated memory must be freed with a call to freeAligned().
 *
 * NOTE: This function is as safe as ::malloc().
 *
 * @param mallocFunc Function with the signature void*(size_t), which is called to allocate memory.
 */
template<class MallocFunc>
void* mallocAligned(size_t size, size_t alignment, MallocFunc mallocFunc)
{
    if (alignment == 0)
        return 0;
    void* ptr = mallocFunc(size + alignment + sizeof(alignment));
    if (!ptr) //< allocation error
        return ptr;

    void* aligned_ptr = (char*) ptr + sizeof(alignment); //< Leaving place to save unalignment.
    const size_t unalignment = alignment - (((size_t) aligned_ptr) % alignment);
    memcpy((char*) ptr+unalignment, &unalignment, sizeof(unalignment));
    return (char*) aligned_ptr + unalignment;
}

/** Calls mallocAligned() passing ::malloc as mallocFunc. */
inline void* mallocAligned(size_t size, size_t alignment)
{
    return mallocAligned<>(size, alignment, ::malloc);
}

/**
 * Free ptr allocated with a call to mallocAligned().
 *
 * NOTE: This function is as safe as ::free().
 *
 * @param freeFunc Function with the signature void(void*), which is called to free the memory.
 */
template<class FreeFunc>
void freeAligned(void* ptr, FreeFunc freeFunc)
{
    if (!ptr)
        return freeFunc(ptr);

    ptr = (char*) ptr - sizeof(size_t);
    size_t unalignment = 0;
    memcpy(&unalignment, ptr, sizeof(unalignment));
    ptr = (char*) ptr - unalignment;

    freeFunc(ptr);
}

/** Calls freeAligned() passing ::free as freeFunc. */
inline void freeAligned(void* ptr)
{
    return freeAligned<>(ptr, ::free);
}

} // namespace utils
} // namespace kit
} // namespace nx
