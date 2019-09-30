#pragma once

#include <stdint.h>
#include <map>
#include <mutex>

#include "abstract_allocator.h"

//#define CA_DEBUG

/**
 * Allocates memory from cyclic buffer. Suitable for media frames allocation,
 * since media frames generally freed in the same order they were allocated.
 * Internally, it is list of buffers, called arena.
 * Overhead per one allocation is 2*sizeof(size_t)
 * NOTE: Maximum possible memory fragmentation is one arena size.
 */
class NX_UTILS_API CyclicAllocator:
    public QnAbstractAllocator
{
public:
    static const size_t DEFAULT_ARENA_SIZE = 4 * 1024 * 1024;

    CyclicAllocator(size_t arenaSize = DEFAULT_ARENA_SIZE);
    ~CyclicAllocator();

    virtual void* alloc(size_t size) override;
    virtual void release(void* ptr) override;

    /**
     * @return Sum of sizes of all arenas. That is total amount of bytes allocated in the heap.
     */
    std::size_t totalBytesAllocated() const;

private:
    class Arena
    {
    public:
        uint8_t* mem;
        size_t size;
        Arena* next;

        Arena(size_t size);
        ~Arena();

        Arena(const Arena&) = delete;
        Arena& operator=(const Arena&) = delete;
    };

    class Position
    {
    public:
        Arena* arena;
        size_t pos;

        Position();

        bool operator==(const Position& right) const;
        bool operator!=(const Position& right) const;
    };

    mutable std::mutex m_mutex;
    Position m_leftMostAllocatedBlock;
    Position m_freeMemStart;
    size_t m_arenaSize;
    size_t m_totalAllocatedBytes = 0;

    size_t m_allocatedBlockCount;

#ifdef CA_DEBUG
    Arena* m_head;
    size_t m_arenaCount;
    //map<ptr, size>
    std::map<void*, size_t> m_allocatedBlocks;
#endif

    /**
     * @return free space in arena pointed to by m_freeMemStart.arena.
     */
    size_t calcCurrentArenaBytesAvailable() const;
    bool allocateNewArenaBeforeCurrent(size_t spaceRequired, Arena* const prevArena);

    /**
     * Moves m_leftMostAllocatedBlock to the next allocated block.
     */
    void skipFreedBlocks();
};
