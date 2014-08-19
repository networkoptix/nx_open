/**********************************************************
* 27 jun 2014
* a.kolesnikov
***********************************************************/

#include "cyclic_allocator.h"

#include <memory>

#ifdef CA_DEBUG
#include <utils/common/log.h>
#endif


CyclicAllocator::Arena::Arena( size_t _size )
:
    mem( static_cast<uint8_t*>(::malloc( _size )) ),
    size( _size ),
    next( nullptr )
{
}

CyclicAllocator::Arena::~Arena()
{
    ::free( mem );
    mem = nullptr;
}


CyclicAllocator::Position::Position()
:
    arena( nullptr ),
    pos( 0 )
{
}

bool CyclicAllocator::Position::operator==( const Position& right ) const
{
    return (this->arena == right.arena) && (this->pos == right.pos);
}

bool CyclicAllocator::Position::operator!=( const Position& right ) const
{
    return !(*this == right);
}


class AllocationHeader
{
public:
    static const size_t ALLOCATION_HEADER_SIZE = sizeof(size_t) + sizeof(int);
    static const int SPACE_NOT_USED = 0;
    static const int SPACE_USED = 1;
    static const int SPACE_SKIPPED = 2;

    //!allocation size, not including this header
    size_t size;
    //!>0, if block is still used
    int used;

    AllocationHeader( size_t _size = 0, int _used = SPACE_NOT_USED )
    :
        size( _size ),
        used( _used )
    {
    }

    void read( uint8_t* from )
    {
        memcpy( &size, from, sizeof(size) );
        from += sizeof(size);
        memcpy( &used, from, sizeof(used) );
        from += sizeof(used);
    }

    //!Writes to buffer \a *to. Moves \a *to forward for bytes written count
    void write( uint8_t** to )
    {
        memcpy( *to, &size, sizeof(size) );
        *to += sizeof(size);
        memcpy( *to, &used, sizeof(used) );
        *to += sizeof(used);
    }
};


CyclicAllocator::CyclicAllocator( size_t arenaSize )
:
    m_arenaSize( arenaSize ),
    m_allocatedBlockCount( 0 )
#ifdef CA_DEBUG
    , m_head( nullptr )
    , m_arenaCount( 0 )
#endif
{
}

CyclicAllocator::~CyclicAllocator()
{
    assert( m_leftMostAllocatedBlock == m_freeMemStart );   //all blocks have been freed
}

#ifdef CA_DEBUG
static void validateBlock( const std::map<void*, size_t>& allocatedBlocks, uint8_t* blockPtr, size_t blockSize )
{
    for( auto val: allocatedBlocks )
    {
        assert( !((blockPtr < static_cast<const uint8_t*>(val.first)+val.second) && (val.first < (blockPtr + blockSize))) );
    }
}
#endif

void* CyclicAllocator::alloc( size_t size )
{
    if( size == 0 )
        return nullptr;

    std::unique_lock<std::mutex> lk( m_mutex );

#ifdef CA_DEBUG
    if( m_freeMemStart.arena )
        validateBlock( m_allocatedBlocks, m_freeMemStart.arena->mem + m_freeMemStart.pos, 1 );
#endif

    const size_t spaceRequired = size + AllocationHeader::ALLOCATION_HEADER_SIZE;
    const size_t currentArenaBytesAvailable = calcCurrentArenaBytesAvailable();
    if( currentArenaBytesAvailable < spaceRequired )   //current arena does not have enough space?
    {
        //moving to the next arena
        const Position freeMemStartBak = m_freeMemStart;    //saving current state for rollback in case of allocation error
        Arena* const prevArena = m_freeMemStart.arena;

        if( m_freeMemStart.arena )
        {
            //marking space left in arena as not used
            if( currentArenaBytesAvailable >= AllocationHeader::ALLOCATION_HEADER_SIZE )
            {
                AllocationHeader allocationHeader( 0, AllocationHeader::SPACE_SKIPPED );
                uint8_t* posPtr = m_freeMemStart.arena->mem + m_freeMemStart.pos;
                allocationHeader.write( &posPtr );
            }

            //moving to the next arena
            m_freeMemStart.arena = m_freeMemStart.arena->next;
            m_freeMemStart.pos = 0;
        }

        //checking, if new arena has enough space
        const size_t nextArenaBytesAvailable = calcCurrentArenaBytesAvailable();
        //switching to empty arena only so that we do not have to deal with linking from middle of arena to another arena
        if( !m_freeMemStart.arena || nextArenaBytesAvailable < std::max<size_t>(spaceRequired, m_freeMemStart.arena->size) )
        {
            if( !allocateNewArenaBeforeCurrent( spaceRequired, prevArena ) )
            {
                m_freeMemStart = freeMemStartBak;   //rolling back changes done in this method
                return nullptr;
            }
        }
    }

    //current arena has enough empty space, allocating
    AllocationHeader header( size, AllocationHeader::SPACE_USED );
    uint8_t* posPtr = m_freeMemStart.arena->mem + m_freeMemStart.pos;
    header.write( &posPtr );
    m_freeMemStart.pos = posPtr - m_freeMemStart.arena->mem;
    m_freeMemStart.pos += size;

    ++m_allocatedBlockCount;

#ifdef CA_DEBUG
    validateBlock( m_allocatedBlocks, posPtr - AllocationHeader::ALLOCATION_HEADER_SIZE, spaceRequired );
    m_allocatedBlocks.emplace( posPtr - AllocationHeader::ALLOCATION_HEADER_SIZE, spaceRequired );

    NX_LOG( lit("Allocated %1 bytes at %2. free mem start pos %3, arena %4").
        arg(size).arg((size_t)posPtr, 0, 16).arg(m_freeMemStart.pos).arg((size_t)m_freeMemStart.arena, 0, 16), cl_logDEBUG1 );
#endif

    return posPtr;
}

void CyclicAllocator::release( void* ptr )
{
    if( !ptr )
        return;

    std::unique_lock<std::mutex> lk( m_mutex );

    uint8_t* allocationHeaderPtr = static_cast<uint8_t*>(ptr) - AllocationHeader::ALLOCATION_HEADER_SIZE;

#ifdef CA_DEBUG
    if( m_freeMemStart.arena )
        validateBlock( m_allocatedBlocks, m_freeMemStart.arena->mem + m_freeMemStart.pos, 1 );
#endif

    AllocationHeader header;
    header.read( allocationHeaderPtr );
    assert( header.used == AllocationHeader::SPACE_USED );
    header.used = AllocationHeader::SPACE_NOT_USED;
    header.write( &allocationHeaderPtr );

#ifdef CA_DEBUG
    if( m_freeMemStart.arena )
        validateBlock( m_allocatedBlocks, m_freeMemStart.arena->mem + m_freeMemStart.pos, 1 );
#endif

#ifdef CA_DEBUG
    m_allocatedBlocks.erase( static_cast<uint8_t*>(ptr) - AllocationHeader::ALLOCATION_HEADER_SIZE );
    NX_LOG( lit("Released block %1 of size %2. Left most allocated block is at %3, arena %4").
        arg((size_t)ptr, 0, 16).arg(header.size).
        arg(m_leftMostAllocatedBlock.pos).arg((size_t)m_leftMostAllocatedBlock.arena, 0, 16), cl_logDEBUG1 );
#endif

    skipFreedBlocks();

    --m_allocatedBlockCount;
}

size_t CyclicAllocator::calcCurrentArenaBytesAvailable() const
{
    if( !m_freeMemStart.arena )
        return 0;

    if( m_freeMemStart.arena != m_leftMostAllocatedBlock.arena )
        return m_freeMemStart.arena->size - m_freeMemStart.pos;

    if( m_freeMemStart == m_leftMostAllocatedBlock )    //head == tail
        return m_allocatedBlockCount > 0
            ? 0                            //arena is full
            : (m_freeMemStart.arena->size - m_freeMemStart.pos);    //arena is empty, but we do not switch from end of arena two beginning of same arena.
                                                                    //Reason for that is to have garanteed maximum memory fragmentation size of one arena 
                                                                    //and make things simplier (at least, for now)

    return (m_freeMemStart.pos > m_leftMostAllocatedBlock.pos
                ? m_freeMemStart.arena->size
                : m_leftMostAllocatedBlock.pos)
            - m_freeMemStart.pos;
}

bool CyclicAllocator::allocateNewArenaBeforeCurrent( size_t spaceRequired, Arena* const prevArena )
{
    //allocating new arena before current
    const size_t newArenaSize = std::max<size_t>( m_arenaSize, spaceRequired );
    std::unique_ptr<Arena> newArena( new Arena( newArenaSize ) );
    if( !newArena->mem )
        return false;
    if( m_arenaSize < newArenaSize )
        m_arenaSize = newArenaSize; //updating arena size only in case of successfull arena allocation
    if( prevArena )
    {
        prevArena->next = newArena.get();
        newArena->next = m_freeMemStart.arena;
    }
    else
    {
        //just allocated first arena
#ifdef CA_DEBUG
        m_head = newArena.get();
#endif
        newArena->next = newArena.get();   //looping first arena to itself
        m_leftMostAllocatedBlock.arena = newArena.get();
        m_leftMostAllocatedBlock.pos = 0;
    }
    m_freeMemStart.arena = newArena.release();
    m_freeMemStart.pos = 0;
#ifdef CA_DEBUG
    ++m_arenaCount;
#endif
    return true;
}

void CyclicAllocator::skipFreedBlocks()
{
    while( m_leftMostAllocatedBlock != m_freeMemStart )
    {
        if( m_leftMostAllocatedBlock.arena->size - m_leftMostAllocatedBlock.pos < AllocationHeader::ALLOCATION_HEADER_SIZE )
        {
            //moving to the next arena
            m_leftMostAllocatedBlock.arena = m_leftMostAllocatedBlock.arena->next;
            m_leftMostAllocatedBlock.pos = 0;
            continue;
        }

        AllocationHeader header;
        header.read( m_leftMostAllocatedBlock.arena->mem + m_leftMostAllocatedBlock.pos );

        switch( header.used )
        {
            case AllocationHeader::SPACE_USED:
                return; //found used block

            case AllocationHeader::SPACE_NOT_USED:
                //skipping unused block
#ifdef CA_DEBUG
                validateBlock( m_allocatedBlocks, m_leftMostAllocatedBlock.arena->mem + m_leftMostAllocatedBlock.pos, 1 );
#endif
                m_leftMostAllocatedBlock.pos += AllocationHeader::ALLOCATION_HEADER_SIZE;
                m_leftMostAllocatedBlock.pos += header.size;
                continue;

            case AllocationHeader::SPACE_SKIPPED:
                //moving to the next arena
                m_leftMostAllocatedBlock.arena = m_leftMostAllocatedBlock.arena->next;
                m_leftMostAllocatedBlock.pos = 0;
                continue;

            default:
                assert( false );
        }
    }
}
