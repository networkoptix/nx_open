/**********************************************************
* 23 jun 2014
* akolesnikov
***********************************************************/

#include "third_party_video_data_packet.h"


QnThirdPartyCompressedVideoData::QnThirdPartyCompressedVideoData(
    nxcip::VideoDataPacket* videoPacket,
    QnMediaContextPtr ctx )
:
    QnCompressedVideoData( ctx ),
    m_videoPacket( videoPacket )
{
}

QnThirdPartyCompressedVideoData::~QnThirdPartyCompressedVideoData()
{
    m_videoPacket->releaseRef();
    m_videoPacket = nullptr;
}

QnWritableCompressedVideoData* QnThirdPartyCompressedVideoData::clone() const
{
    QnWritableCompressedVideoData* cloned = new QnWritableCompressedVideoData();
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
