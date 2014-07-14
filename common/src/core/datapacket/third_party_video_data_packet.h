/**********************************************************
* 23 jun 2014
* akolesnikov
***********************************************************/

#ifndef THIRD_PARTY_VIDEO_DATA_PACKET_H
#define THIRD_PARTY_VIDEO_DATA_PACKET_H

#ifdef ENABLE_DATA_PROVIDERS

#include "video_data_packet.h"

#include <plugins/camera_plugin.h>


class QnThirdPartyCompressedVideoData
:
    public QnCompressedVideoData
{
public:
    /*!
        \note Does not call \a videoPacket->addRef
    */
    QnThirdPartyCompressedVideoData(
        nxcip::VideoDataPacket* videoPacket,
        QnMediaContextPtr ctx = QnMediaContextPtr(0) );
    /*!
        \note Calls \a videoPacket->releaseRef
    */
    virtual ~QnThirdPartyCompressedVideoData();

    //!Implementation of QnAbstractMediaData::clone
    virtual QnWritableCompressedVideoData* clone( QnAbstractAllocator* allocator = QnSystemAllocator::instance() ) const override;
    //!Implementation of QnAbstractMediaData::data
    virtual const char* data() const override;
    //!Implementation of QnAbstractMediaData::dataSize
    virtual size_t dataSize() const override;

private:
    nxcip::VideoDataPacket* m_videoPacket;
};

#endif // ENABLE_DATA_PROVIDERS

#endif //THIRD_PARTY_VIDEO_DATA_PACKET_H

