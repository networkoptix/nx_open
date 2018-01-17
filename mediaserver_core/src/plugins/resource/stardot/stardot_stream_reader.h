#ifndef _STARDOT_STREAM_REDER_H__
#define _STARDOT_STREAM_REDER_H__

#ifdef ENABLE_STARDOT

#include "providers/spush_media_stream_provider.h"
#include "network/multicodec_rtp_reader.h"
#include "core/resource/resource_media_layout.h"

class QnStardotStreamReader: public CLServerPushStreamReader
{
public:
    QnStardotStreamReader(const QnResourcePtr& res);
    virtual ~QnStardotStreamReader();

    QnConstResourceAudioLayoutPtr getDPAudioLayout() const;

protected:
    virtual QnAbstractMediaDataPtr getNextData() override;
    virtual CameraDiagnostics::Result openStreamInternal(bool isCameraControlRequired, const QnLiveStreamParams& params) override;
    virtual void closeStream() override;
    virtual bool isStreamOpened() const override;

    virtual void pleaseStop() override;
    virtual QnMetaDataV1Ptr getCameraMetadata() override;
private:
    void parseMotionInfo(const QnCompressedVideoDataPtr& videoData);
    void processMotionBinData(const quint8* data, qint64 timestamp);
private:
    QnMulticodecRtpReader m_multiCodec;
    QnStardotResourcePtr m_stardotRes;
    QnMetaDataV1Ptr m_lastMetadata;
};

#endif // #ifdef ENABLE_STARDOT
#endif // _STARDOT_STREAM_REDER_H__
