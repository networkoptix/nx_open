/**********************************************************
* 20 jun 2014
* a.kolesnikov
***********************************************************/

#include "nx_allocation_cache.h"

#include <algorithm>


static const size_t MAX_PERCENT = 100;

NxBufferCache::NxBufferCache(
    size_t maxUnusedMemoryToCache,
    size_t maxBufferSizeExcessPercent,
    size_t bufferAllocationExcessPercent )
:
    m_maxUnusedMemoryToCache( maxUnusedMemoryToCache ),
    m_maxBufferSizeExcessPercent( maxBufferSizeExcessPercent ),
    m_bufferAllocationExcessPercent( bufferAllocationExcessPercent ),
    m_totalCacheSize( 0 )
{
}

NxBufferCache::~NxBufferCache()
{
    for( auto val: m_freeBuffers )
        ::free( static_cast<char*>(val.second) - sizeof(size_t) );
    m_freeBuffers.clear();
}

void* NxBufferCache::getBuffer( size_t size )
{
    std::unique_lock<std::mutex> lk( m_mutex );

    //looking for an already allocated buffer suitable for request
    auto it = std::lower_bound( m_freeBuffers.begin(), m_freeBuffers.end(), std::pair<size_t, void*>(size, nullptr) );
    if( (it != m_freeBuffers.end()) &&
        (it->first < (size + (size / MAX_PERCENT) * m_maxBufferSizeExcessPercent)) )
    {
        //found it
        assert( it->first >= size );
        void* ptr = it->second;
        m_totalCacheSize -= it->first;
        m_freeBuffers.erase( it );  //TODO #ak const erase complexity desired
        return ptr;
    }

    //allocating new buffer
    const size_t bufferToAllocateSize = size + (size / MAX_PERCENT * m_bufferAllocationExcessPercent);
    void* newBuf = ::malloc( bufferToAllocateSize + sizeof(bufferToAllocateSize) );
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
    std::pair<size_t, void*> newElem(size, ptr);
    auto it = std::upper_bound( m_freeBuffers.begin(), m_freeBuffers.end(), newElem );
    m_freeBuffers.emplace( it, newElem );
    m_totalCacheSize += size;
}

void NxBufferCache::cleanCacheIfNeeded()
{
    while( m_totalCacheSize > m_maxUnusedMemoryToCache )
    {
        //erasing largest buffer first
        //TODO #ak should erases oldest buffer first, not largest!
        assert( !m_freeBuffers.empty() );
        const auto& it = m_freeBuffers.rbegin();
        m_totalCacheSize -= it->first;
        ::free( static_cast<char*>(it->second) - sizeof(size_t) );
        m_freeBuffers.erase( std::prev(it.base()) );
    }
}
