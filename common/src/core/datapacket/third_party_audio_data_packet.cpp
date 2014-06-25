/**********************************************************
* 23 jun 2014
* akolesnikov
***********************************************************/

#include "third_party_audio_data_packet.h"


QnThirdPartyCompressedAudioData::QnThirdPartyCompressedAudioData(
    nxcip::MediaDataPacket* audioPacket,
    QnMediaContextPtr ctx )
:
    QnCompressedAudioData( ctx ),
    m_audioPacket( audioPacket )
{
}

QnThirdPartyCompressedAudioData::~QnThirdPartyCompressedAudioData()
{
    m_audioPacket->releaseRef();
    m_audioPacket = nullptr;
}

QnWritableCompressedAudioData* QnThirdPartyCompressedAudioData::clone() const
{
    QnWritableCompressedAudioData* cloned = new QnWritableCompressedAudioData();
    cloned->QnCompressedAudioData::assign( this );
    cloned->m_data.write( static_cast<const char*>(m_audioPacket->data()), m_audioPacket->dataSize() );
    return cloned;
}

const char* QnThirdPartyCompressedAudioData::data() const
{
    return static_cast<const char*>(m_audioPacket->data());
}

size_t QnThirdPartyCompressedAudioData::dataSize() const
{
    return m_audioPacket->dataSize();
}
