/**********************************************************
* 23 jun 2014
* akolesnikov
***********************************************************/

#include "video_data_packet.h"

#include <utils/memory/cyclic_allocator.h>
#include <network/h264_rtp_parser.h>

QnCompressedVideoData::QnCompressedVideoData( QnConstMediaContextPtr ctx )
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

//static CyclicAllocator videoPacketCyclicAllocator;

QnWritableCompressedVideoData::QnWritableCompressedVideoData(
    unsigned int alignment,
    unsigned int capacity,
    QnConstMediaContextPtr ctx )
:   //TODO #ak delegate constructor (requires msvc2013)
    QnCompressedVideoData( ctx ),
    m_data(/*&videoPacketCyclicAllocator,*/ alignment, capacity)
{
    Q_ASSERT(capacity <= MAX_ALLOWED_FRAME_SIZE);
}

QnWritableCompressedVideoData::QnWritableCompressedVideoData(
    QnAbstractAllocator* allocator,
    unsigned int alignment,
    unsigned int capacity,
    QnConstMediaContextPtr ctx )
:
    QnCompressedVideoData( ctx ),
    m_data(allocator, alignment, capacity)
{
    Q_ASSERT(capacity <= MAX_ALLOWED_FRAME_SIZE);
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
