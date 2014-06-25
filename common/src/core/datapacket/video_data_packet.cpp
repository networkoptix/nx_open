/**********************************************************
* 23 jun 2014
* akolesnikov
***********************************************************/

#include "video_data_packet.h"


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

//!Implementation of QnAbstractMediaData::clone
QnCompressedVideoData* QnCompressedVideoData::clone() const 
{
    assert( false );
    return nullptr;
}

void QnCompressedVideoData::assign(const QnCompressedVideoData* other)
{
    QnAbstractMediaData::assign(other);
    width = other->width;
    height = other->height;
    motion = other->motion;
    pts = other->pts;
}


static const unsigned int MAX_VIDEO_PACKET_SIZE = 10 * 1024 * 1024;

QnWritableCompressedVideoData::QnWritableCompressedVideoData(
    unsigned int alignment,
    unsigned int capacity,
    QnMediaContextPtr ctx )
:
    QnCompressedVideoData( ctx ),
    m_data(alignment, qMin(capacity, MAX_VIDEO_PACKET_SIZE))
{
}

//!Implementation of QnAbstractMediaData::clone
QnWritableCompressedVideoData* QnWritableCompressedVideoData::clone() const
{
    QnWritableCompressedVideoData* rez = new QnWritableCompressedVideoData(m_data.getAlignment(), m_data.size());
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
