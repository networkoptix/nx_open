/**********************************************************
* 23 jun 2014
* akolesnikov
***********************************************************/

#include "third_party_video_data_packet.h"

#include <utils/common/log.h>


QnThirdPartyCompressedVideoData::QnThirdPartyCompressedVideoData(
    nxcip::VideoDataPacket* videoPacket,
    QnMediaContextPtr ctx )
:
    QnCompressedVideoData( ctx ),
    m_videoPacket( videoPacket )
{
    NX_LOG( lit("QnThirdPartyCompressedVideoData::QnThirdPartyCompressedVideoData.  %1").arg((size_t)this), cl_logDEBUG1 );
}

QnThirdPartyCompressedVideoData::~QnThirdPartyCompressedVideoData()
{
    NX_LOG( lit("QnThirdPartyCompressedVideoData::~QnThirdPartyCompressedVideoData. %1").arg((size_t)this), cl_logDEBUG1 );

    m_videoPacket->releaseRef();
    m_videoPacket = nullptr;
}

QnWritableCompressedVideoData* QnThirdPartyCompressedVideoData::clone( QnAbstractAllocator* allocator ) const
{
    QnWritableCompressedVideoData* cloned = new QnWritableCompressedVideoData(allocator);
    cloned->QnCompressedVideoData::assign( this );
    cloned->m_data.write( static_cast<const char*>(m_videoPacket->data()), m_videoPacket->dataSize() );
    return cloned;
}

const char* QnThirdPartyCompressedVideoData::data() const
{
    return static_cast<const char*>(m_videoPacket->data());
}

size_t QnThirdPartyCompressedVideoData::dataSize() const
{
    return m_videoPacket->dataSize();
}
