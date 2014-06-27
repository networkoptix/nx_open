/**********************************************************
* 27 jun 2014
* a.kolesnikov
***********************************************************/

#ifndef CYCLIC_ALLOCATOR_H
#define CYCLIC_ALLOCATOR_H

#include <stdint.h>
#include <mutex>

#include "abstract_allocator.h"


//!Allocates memory from cyclic buffer. Suitable for media frames allocation, since media frames generally freed in the same order they were allocated
/*!
    Internally, it is list of buffers, called arena.
    Overhead per one allocation is 2*sizeof(size_t)
*/
class CyclicAllocator
:
    public QnAbstractAllocator
{
public:
    static const size_t DEFAULT_ARENA_SIZE = 8*1024*1024;

    CyclicAllocator( size_t arenaSize = DEFAULT_ARENA_SIZE );
    ~CyclicAllocator();

    virtual void* alloc( size_t size ) override;
    virtual void release( void* ptr ) override;

private:
    //!Allocation area
    class Arena
    {
    public:
        uint8_t* mem;
        size_t size;
        Arena* next;

        Arena( size_t size );
        ~Arena();
    };

    class Position
    {
    public:
        Arena* arena;
        size_t pos;

        Position();

        bool operator==( const Position& right ) const;
        bool operator!=( const Position& right ) const;
    };

    std::mutex m_mutex;
    Position m_leftMostAllocatedBlock;
    Position m_freeMemStart;
    size_t m_arenaSize;

    Arena* m_head;
    size_t m_allocatedBlocks;

    //!Returns free space in arena pointed to by \a m_freeMemStart.arena
    size_t calcCurrentArenaBytesAvailable() const;
    bool allocateNewArenaBeforeCurrent( size_t spaceRequired, Arena* const prevArena );
    //!Moves \a m_leftMostAllocatedBlock to the next allocated block
    void skipFreedBlocks();
};

#endif  //CYCLIC_ALLOCATOR_H
