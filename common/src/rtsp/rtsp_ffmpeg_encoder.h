#pragma once

#ifdef ENABLE_DATA_PROVIDERS

#include <QtCore/QMap>

#include "rtsp_encoder.h"
#include <transcoding/ffmpeg_video_transcoder.h>

static const quint8 RTP_FFMPEG_GENERIC_CODE = 102;
static const QString RTP_FFMPEG_GENERIC_STR(lit("FFMPEG"));

class QnRtspFfmpegEncoder: public QnRtspEncoder, public QnCommonModuleAware
{
public:
    QnRtspFfmpegEncoder(QnCommonModule* commonModule);

    virtual QString getSdpMedia(bool isVideo, int trackId) override;

    void setCodecContext(const QnConstMediaContextPtr& codecContext);

    virtual void setDataPacket(QnConstAbstractMediaDataPtr media) override;
    virtual bool getNextPacket(QnByteArray& sendBuffer) override;
    virtual void init() override;

    virtual quint32 getSSRC() override;
    virtual bool getRtpMarker() override;
    virtual quint32 getFrequency() override;
    virtual quint8 getPayloadType() override;
    virtual QString getName() override;

    void setDstResolution(const QSize& dstVideSize, AVCodecID dstCodec);

    void setLiveMarker(int value);
    void setAdditionFlags(quint16 value);

    virtual bool isRtpHeaderExists() const override { return false; }
private:
    bool m_gotLivePacket;
    QnConstMediaContextPtr m_contextSent;
    QMap<AVCodecID, QnConstMediaContextPtr> m_generatedContexts;
    QnConstAbstractMediaDataPtr m_media;
    const char* m_curDataBuffer;
    QByteArray m_codecCtxData;
    int m_liveMarker;
    quint16 m_additionFlags;
    bool m_eofReached;
    bool m_isLastDataContext;
    QSize m_dstVideSize;
    AVCodecID m_dstCodec;

    std::unique_ptr<QnFfmpegVideoTranscoder> m_videoTranscoder;

    QnConstMediaContextPtr getGeneratedContext(AVCodecID compressionType);
    QnConstAbstractMediaDataPtr transcodeVideoPacket(QnConstAbstractMediaDataPtr media);
};

using QnRtspFfmpegEncoderPtr = std::unique_ptr<QnRtspFfmpegEncoder>;

#endif // ENABLE_DATA_PROVIDERS
