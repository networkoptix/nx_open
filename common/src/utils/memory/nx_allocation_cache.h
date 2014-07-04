/**********************************************************
* 20 jun 2014
* a.kolesnikov
***********************************************************/

#ifndef NX_ALLOCATION_CACHE_H
#define NX_ALLOCATION_CACHE_H

#include <deque>
#include <mutex>


/*!
    Introduced to solve problem of large memory fragmentation on ARM11 platform.
    Based on the fact that video frames of same stream do generally have almost same size across GOPs
    \note This class is intended to be used for allocating relatively large buffers (e.g., video packets)
*/
class NxBufferCache
{
public:
    //!When searching for an existing buffer, buffer with size MAX_BUFFER_SIZE_EXCESS_PERCENT percent greater than requested will do
    static const size_t DEFAULT_MAX_BUFFER_SIZE_EXCESS_PERCENT = 15;
    //!New buffer is allocated this value percent larger than requested
    static const size_t DEFAULT_BUFFER_ALLOCATION_EXCESS_PERCENT = 10;
    static const size_t DEFAULT_MAX_UNUSED_MEMORY_TO_CACHE = 5*1024*1024;

    NxBufferCache(
        size_t maxUnusedMemoryToCache = DEFAULT_MAX_UNUSED_MEMORY_TO_CACHE,
        size_t maxBufferSizeExcessPercent = DEFAULT_MAX_BUFFER_SIZE_EXCESS_PERCENT,
        size_t bufferAllocationExcessPercent = DEFAULT_BUFFER_ALLOCATION_EXCESS_PERCENT );
    ~NxBufferCache();

    void* getBuffer( size_t size );
    void release( void* ptr );

    void setCacheSize( size_t maxUnusedMemoryToCache );

    struct BufferContext
    {
        size_t size;
        void* ptr;
        size_t timestamp;

        BufferContext( size_t _size = 0, void* _ptr = nullptr, size_t _timestamp = 0 )
        :
            size( _size ),
            ptr( _ptr ),
            timestamp( _timestamp )
        {
        }
    };

private:
    size_t m_maxUnusedMemoryToCache;
    const size_t m_maxBufferSizeExcessPercent;
    const size_t m_bufferAllocationExcessPercent;
    std::mutex m_mutex;
    //!map<size, ptr>. storing only unused buffers here
    std::deque<BufferContext> m_freeBuffers;
    size_t m_totalCacheSize;
    size_t m_prevTimestamp;

    void cleanCacheIfNeeded();
};

#endif  //NX_ALLOCATION_CACHE_H
