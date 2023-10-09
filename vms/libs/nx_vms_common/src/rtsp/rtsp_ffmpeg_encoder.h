// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMap>

#include <nx/utils/software_version.h>
#include <transcoding/ffmpeg_video_transcoder.h>

#include "abstract_rtsp_encoder.h"

class NX_VMS_COMMON_API QnRtspFfmpegEncoder: public AbstractRtspEncoder
{
public:
    QnRtspFfmpegEncoder(const DecoderConfig& config, nx::metrics::Storage* metrics);

    virtual QString getSdpMedia(bool isVideo, int trackId, int port = 0) override;

    void setCodecContext(const CodecParametersConstPtr& codecParams);

    virtual void setDataPacket(QnConstAbstractMediaDataPtr media) override;
    virtual bool getNextPacket(nx::utils::ByteArray& sendBuffer) override;
    virtual void init() override;
    virtual bool isEof() const override { return false; }
    virtual void setMtu(int mtu) override;

    void setDstResolution(const QSize& dstVideoSize, AVCodecID dstCodec);

    void setLiveMarker(int value);
    void setAdditionFlags(quint16 value);

    void setServerVersion(const nx::utils::SoftwareVersion& serverVersion);

private:
    DecoderConfig m_config;
    bool m_gotLivePacket;
    CodecParametersConstPtr m_contextSent;
    QMap<AVCodecID, CodecParametersConstPtr> m_generatedContexts;
    QnConstAbstractMediaDataPtr m_media;
    const char* m_curDataBuffer;
    QByteArray m_codecParamsData;
    int m_liveMarker;
    quint16 m_additionFlags;
    bool m_eofReached;
    QSize m_dstVideSize;
    uint16_t m_sequence = 0;
    int m_mtu = 0;

    std::unique_ptr<QnFfmpegVideoTranscoder> m_videoTranscoder;
    nx::metrics::Storage* m_metrics = nullptr;
    nx::utils::SoftwareVersion m_serverVersion;

    CodecParametersConstPtr getGeneratedContext(AVCodecID compressionType);
    QnConstAbstractMediaDataPtr transcodeVideoPacket(QnConstAbstractMediaDataPtr media);
};

using QnRtspFfmpegEncoderPtr = std::unique_ptr<QnRtspFfmpegEncoder>;
