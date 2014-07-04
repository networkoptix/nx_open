/**********************************************************
* 23 jun 2014
* akolesnikov
***********************************************************/

#include "video_data_packet.h"

#ifdef ENABLE_DATA_PROVIDERS

#include <utils/memory/cyclic_allocator.h>


QnCompressedVideoData::QnCompressedVideoData( QnMediaContextPtr ctx )
:
    QnAbstractMediaData( VIDEO ),
    width( -1 ),
    height( -1 ),
    pts( AV_NOPTS_VALUE )
{
    //useTwice = false;
    context = ctx;
    //ignore = false;
    flags = 0;
}

void QnCompressedVideoData::assign(const QnCompressedVideoData* other)
{
    QnAbstractMediaData::assign(other);
    width = other->width;
    height = other->height;
    motion = other->motion;
    pts = other->pts;
}


static const unsigned int MAX_VIDEO_PACKET_SIZE = 10*1024*1024;

//static CyclicAllocator videoPacketCyclicAllocator;

QnWritableCompressedVideoData::QnWritableCompressedVideoData(
    unsigned int alignment,
    unsigned int capacity,
    QnMediaContextPtr ctx )
:   //TODO #ak delegate constructor (requires msvc2013)
    QnCompressedVideoData( ctx ),
    m_data(/*&videoPacketCyclicAllocator,*/ alignment, qMin(capacity, MAX_VIDEO_PACKET_SIZE))
{
}

QnWritableCompressedVideoData::QnWritableCompressedVideoData(
    QnAbstractAllocator* allocator,
    unsigned int alignment,
    unsigned int capacity,
    QnMediaContextPtr ctx )
:
    QnCompressedVideoData( ctx ),
    m_data(allocator, alignment, qMin(capacity, MAX_VIDEO_PACKET_SIZE))
{
}

//!Implementation of QnAbstractMediaData::clone
QnWritableCompressedVideoData* QnWritableCompressedVideoData::clone( QnAbstractAllocator* allocator ) const
{
    QnWritableCompressedVideoData* rez = new QnWritableCompressedVideoData(
        allocator,
        m_data.getAlignment(),
        m_data.size() );
    rez->assign(this);
    return rez;
}

//!Implementation of QnAbstractMediaData::data
const char* QnWritableCompressedVideoData::data() const
{
    return m_data.data();
}

//!Implementation of QnAbstractMediaData::dataSize
size_t QnWritableCompressedVideoData::dataSize() const
{
    return m_data.size();
}

void QnWritableCompressedVideoData::assign( const QnWritableCompressedVideoData* other )
{
    QnCompressedVideoData::assign( other );
    m_data = other->m_data;
}

#endif // ENABLE_DATA_PROVIDERS
