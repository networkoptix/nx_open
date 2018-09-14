/**********************************************************
* 23 jun 2014
* akolesnikov
***********************************************************/

#pragma once

#ifdef ENABLE_DATA_PROVIDERS

#include <nx/streaming/video_data_packet.h>

#include <camera/camera_plugin.h>

class QnThirdPartyCompressedVideoData
:
    public QnCompressedVideoData
{
public:
    QnThirdPartyCompressedVideoData(const QnThirdPartyCompressedVideoData&) = delete;
    QnThirdPartyCompressedVideoData& operator=(const QnThirdPartyCompressedVideoData&) = delete;
    QnThirdPartyCompressedVideoData(QnThirdPartyCompressedVideoData&&) = delete;
    QnThirdPartyCompressedVideoData& operator=(QnThirdPartyCompressedVideoData&&) = delete;

    /*!
        \note Does not call \a videoPacket->addRef
    */
    QnThirdPartyCompressedVideoData(
        nxcip::MediaDataPacket* videoPacket,
        QnConstMediaContextPtr ctx = QnConstMediaContextPtr(nullptr) );
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
    nxcip::MediaDataPacket* m_mediaPacket;
};

#endif // ENABLE_DATA_PROVIDERS
