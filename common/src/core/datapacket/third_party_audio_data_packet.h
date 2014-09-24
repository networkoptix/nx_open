/**********************************************************
* 23 jun 2014
* akolesnikov
***********************************************************/

#ifndef THIRD_PARTY_AUDIO_DATA_PACKET_H
#define THIRD_PARTY_AUDIO_DATA_PACKET_H

#ifdef ENABLE_DATA_PROVIDERS

#include "audio_data_packet.h"

#include <plugins/camera_plugin.h>


class QnThirdPartyCompressedAudioData
:
    public QnCompressedAudioData
{
public:
    /*!
        \note Does not call \a videoPacket->addRef
    */
    QnThirdPartyCompressedAudioData(
        nxcip::MediaDataPacket* videoPacket,
        QnMediaContextPtr ctx = QnMediaContextPtr(0) );
    /*!
        \note Calls \a videoPacket->releaseRef
    */
    virtual ~QnThirdPartyCompressedAudioData();

    //!Implementation of QnAbstractMediaData::clone
    virtual QnWritableCompressedAudioData* clone( QnAbstractAllocator* allocator = QnSystemAllocator::instance() ) const override;
    //!Implementation of QnAbstractMediaData::data
    virtual const char* data() const override;
    //!Implementation of QnAbstractMediaData::dataSize
    virtual size_t dataSize() const override;

private:
    nxcip::MediaDataPacket* m_audioPacket;
};


#endif // ENABLE_DATA_PROVIDERS

#endif  //THIRD_PARTY_AUDIO_DATA_PACKET_H

