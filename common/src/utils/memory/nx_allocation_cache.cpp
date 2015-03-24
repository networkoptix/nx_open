/**********************************************************
* 20 jun 2014
* a.kolesnikov
***********************************************************/

#include "nx_allocation_cache.h"

#include <algorithm>

//#define DEBUG_OUTPUT

//TODO #ak overall optimization of this class is required: 
    // - const complexity of erase
    // - binary complexity of search/add
    // - elements should be iteratable by timestamp growth

static const size_t MAX_PERCENT = 100;

static bool cmp_lb( const NxBufferCache::BufferContext& buf, size_t size )
{
    return buf.size < size;
}

static bool cmp_ub( size_t size, const NxBufferCache::BufferContext& buf )
{
    return size < buf.size;
}

NxBufferCache::NxBufferCache(
    size_t maxUnusedMemoryToCache,
    size_t maxBufferSizeExcessPercent,
    size_t bufferAllocationExcessPercent )
:
    m_maxUnusedMemoryToCache( maxUnusedMemoryToCache ),
    m_maxBufferSizeExcessPercent( maxBufferSizeExcessPercent ),
    m_bufferAllocationExcessPercent( bufferAllocationExcessPercent ),
    m_totalCacheSize( 0 ),
    m_prevTimestamp( 0 )
{
}

NxBufferCache::~NxBufferCache()
{
    for( auto val: m_freeBuffers )
        ::free( static_cast<char*>(val.ptr) - sizeof(size_t) );
    m_freeBuffers.clear();
}

void* NxBufferCache::getBuffer( size_t size )
{
    std::unique_lock<std::mutex> lk( m_mutex );

    //looking for an already allocated buffer suitable for request
    auto it = std::lower_bound( m_freeBuffers.begin(), m_freeBuffers.end(), size, cmp_lb );
    if( (it != m_freeBuffers.end()) &&
        (it->size < (size + (size / MAX_PERCENT) * m_maxBufferSizeExcessPercent)) )
    {
        //found it
        assert( it->size >= size );
        void* ptr = it->ptr;
        m_totalCacheSize -= it->size;
#ifdef DEBUG_OUTPUT
        std::cout<<"NxBufferCache. Cache hit! Returning "<<it->size<<" buffer ("<<size<<" bytes requested). "
            "totalCacheSize "<<m_totalCacheSize<<" ("<<m_freeBuffers.size()<<")"<<std::endl;
#endif
        m_freeBuffers.erase( it );  //TODO #ak const erase complexity desired
        return ptr;
    }

    //allocating new buffer
    const size_t bufferToAllocateSize = size + (size / MAX_PERCENT * m_bufferAllocationExcessPercent);
    void* newBuf = ::malloc( bufferToAllocateSize + sizeof(bufferToAllocateSize) );
#ifdef DEBUG_OUTPUT
    std::cout<<"NxBufferCache. Allocating new buffer "<<size<<"("<<bufferToAllocateSize<<") bytes size. "
        "totalCacheSize "<<m_totalCacheSize<<" ("<<m_freeBuffers.size()<<")"<<std::endl;
#endif
    if( !newBuf )
        return nullptr;

    //saving size
    //TODO #ak should perform aligned write only
    memcpy( newBuf, &bufferToAllocateSize, sizeof(bufferToAllocateSize) );
    return static_cast<char*>(newBuf) + sizeof(bufferToAllocateSize);
}

void NxBufferCache::release( void* ptr )
{
    if( !ptr )
        return;

    //retrieving buffer size
    size_t size = 0;
    //TODO #ak size should be written on aligned address
    memcpy( &size, static_cast<char*>(ptr) - sizeof(size), sizeof(size) );
    std::unique_lock<std::mutex> lk( m_mutex );
    cleanCacheIfNeeded();
    BufferContext newElem( size, ptr, ++m_prevTimestamp );
    auto it = std::upper_bound( m_freeBuffers.begin(), m_freeBuffers.end(), size, cmp_ub );
    m_freeBuffers.emplace( it, newElem );
    m_totalCacheSize += size;
}

void NxBufferCache::setCacheSize( size_t maxUnusedMemoryToCache )
{
    std::unique_lock<std::mutex> lk( m_mutex );
    m_maxUnusedMemoryToCache = maxUnusedMemoryToCache;
#ifdef DEBUG_OUTPUT
    std::cout<<"NxBufferCache::setCacheSize. m_maxUnusedMemoryToCache "<<m_maxUnusedMemoryToCache<<std::endl;
#endif
}

void NxBufferCache::cleanCacheIfNeeded()
{
    while( m_totalCacheSize > m_maxUnusedMemoryToCache )
    {
        assert( !m_freeBuffers.empty() );

        //erasing oldest buffer first
        auto oldestElemIter = std::min_element(
            m_freeBuffers.begin(),
            m_freeBuffers.end(),
            []( const BufferContext& one, const BufferContext& two ){ return one.timestamp < two.timestamp; } );

        m_totalCacheSize -= oldestElemIter->size;
        ::free( static_cast<char*>(oldestElemIter->ptr) - sizeof(size_t) );
        m_freeBuffers.erase( oldestElemIter );
    }
}
