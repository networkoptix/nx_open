#pragma once

#ifdef ENABLE_DATA_PROVIDERS

#include <QtCore/QMap>

#include "abstract_rtsp_encoder.h"
#include <transcoding/ffmpeg_video_transcoder.h>

class QnRtspFfmpegEncoder: public AbstractRtspEncoder
{
public:
    QnRtspFfmpegEncoder(const DecoderConfig& config, nx::metrics::Storage* metrics);

    virtual QString getSdpMedia(bool isVideo, int trackId) override;

    void setCodecContext(const QnConstMediaContextPtr& codecContext);

    virtual void setDataPacket(QnConstAbstractMediaDataPtr media) override;
    virtual bool getNextPacket(QnByteArray& sendBuffer) override;
    virtual void init() override;

    void setDstResolution(const QSize& dstVideSize, AVCodecID dstCodec);

    void setLiveMarker(int value);
    void setAdditionFlags(quint16 value);

private:
    DecoderConfig m_config;
    bool m_gotLivePacket;
    QnConstMediaContextPtr m_contextSent;
    QMap<AVCodecID, QnConstMediaContextPtr> m_generatedContexts;
    QnConstAbstractMediaDataPtr m_media;
    const char* m_curDataBuffer;
    QByteArray m_codecCtxData;
    int m_liveMarker;
    quint16 m_additionFlags;
    bool m_eofReached;
    QSize m_dstVideSize;
    AVCodecID m_dstCodec;
    uint16_t m_sequence = 0;

    std::unique_ptr<QnFfmpegVideoTranscoder> m_videoTranscoder;
    nx::metrics::Storage* m_metrics = nullptr;

    QnConstMediaContextPtr getGeneratedContext(AVCodecID compressionType);
    QnConstAbstractMediaDataPtr transcodeVideoPacket(QnConstAbstractMediaDataPtr media);
};

using QnRtspFfmpegEncoderPtr = std::unique_ptr<QnRtspFfmpegEncoder>;

#endif // ENABLE_DATA_PROVIDERS
