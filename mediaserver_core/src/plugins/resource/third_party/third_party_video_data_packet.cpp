/**********************************************************
* 23 jun 2014
* akolesnikov
***********************************************************/

#include "third_party_video_data_packet.h"

#ifdef ENABLE_DATA_PROVIDERS

#include <nx/utils/log/log.h>


QnThirdPartyCompressedVideoData::QnThirdPartyCompressedVideoData(nxcip::MediaDataPacket* videoPacket,
    QnConstMediaContextPtr ctx )
:
    QnCompressedVideoData( ctx ),
    m_mediaPacket( videoPacket )
{
}

QnThirdPartyCompressedVideoData::~QnThirdPartyCompressedVideoData()
{
    m_mediaPacket->releaseRef();
    m_mediaPacket = nullptr;
}

QnWritableCompressedVideoData* QnThirdPartyCompressedVideoData::clone( QnAbstractAllocator* allocator ) const
{
    QnWritableCompressedVideoData* cloned = new QnWritableCompressedVideoData(allocator);
    cloned->QnCompressedVideoData::assign( this );
    cloned->m_data.write( static_cast<const char*>(m_mediaPacket->data()), m_mediaPacket->dataSize() );
    return cloned;
}

const char* QnThirdPartyCompressedVideoData::data() const
{
    return static_cast<const char*>(m_mediaPacket->data());
}

size_t QnThirdPartyCompressedVideoData::dataSize() const
{
    return m_mediaPacket->dataSize();
}

#endif // ENABLE_DATA_PROVIDERS
