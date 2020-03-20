#pragma once

#ifdef ENABLE_AXIS

#include <providers/spush_media_stream_provider.h>
#include "network/multicodec_rtp_reader.h"
#include "core/resource/resource_media_layout.h"

class QnAxisStreamReader: public CLServerPushStreamReader
{
public:
    QnAxisStreamReader(const QnPlAxisResourcePtr& res);
    virtual ~QnAxisStreamReader();

    virtual QnConstResourceAudioLayoutPtr getDPAudioLayout() const override;

protected:
    virtual QnAbstractMediaDataPtr getNextData() override;
    virtual CameraDiagnostics::Result openStreamInternal(bool isCameraControlRequired, const QnLiveStreamParams& params) override;
    virtual void closeStream() override;
    virtual bool isStreamOpened() const override;
    virtual void pleaseStop() override;

private:
    QnAbstractMediaDataPtr getNextDataMPEG(AVCodecID ci);
    QnAbstractMediaDataPtr getNextDataMJPEG();
    virtual QnMetaDataV1Ptr getCameraMetadata() override;

    QStringList getRTPurls() const;
    int toAxisQuality(Qn::StreamQuality quality);
    void parseMotionInfo(QnCompressedVideoData* videoData);
    void processTriggerData(const quint8* payload, int len);

    void fillMotionInfo(const QRect& rect);
    bool isGotFrame(QnCompressedVideoDataPtr videoData);

private:
    QnMetaDataV1Ptr m_lastMetadata;
    QnMulticodecRtpReader m_rtpStreamParser;
    QnPlAxisResourcePtr m_axisRes;

    bool m_oldFirmwareWarned = false;
};

#endif // #ifdef ENABLE_AXIS
