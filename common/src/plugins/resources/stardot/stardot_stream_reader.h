#ifndef _STARDOT_STREAM_REDER_H__
#define _STARDOT_STREAM_REDER_H__

#include "core/dataprovider/spush_media_stream_provider.h"
#include "utils/network/multicodec_rtp_reader.h"
#include "core/resource/resource_media_layout.h"

class QnStardotStreamReader: public CLServerPushStreamreader
{
public:
    QnStardotStreamReader(QnResourcePtr res);
    virtual ~QnStardotStreamReader();

    const QnResourceAudioLayout* getDPAudioLayout() const;
protected:
    virtual QnAbstractMediaDataPtr getNextData() override;
    virtual void openStream() override;
    virtual void closeStream() override;
    virtual bool isStreamOpened() const override;


    virtual void updateStreamParamsBasedOnQuality() override;
    virtual void updateStreamParamsBasedOnFps() override;

    virtual void pleaseStop() override;
    virtual QnMetaDataV1Ptr getCameraMetadata() override;
private:
    void parseMotionInfo(QnCompressedVideoDataPtr videoData);
    void processMotionBinData(const quint8* data, qint64 timestamp);
private:
    QnMulticodecRtpReader m_multiCodec;
    QnStardotResourcePtr m_stardotRes;
    QnMetaDataV1Ptr m_lastMetadata;
};

#endif // _STARDOT_STREAM_REDER_H__
