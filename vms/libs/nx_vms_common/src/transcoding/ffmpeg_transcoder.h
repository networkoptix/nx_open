// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
}

#include <export/signer.h>
#include <nx/utils/cryptographic_hash.h>

#include "decoders/video/ffmpeg_video_decoder.h"
#include "transcoder.h"
#include "utils/media/frame_info.h"

class QnLicensePool;

class NX_VMS_COMMON_API QnFfmpegTranscoder: public QnTranscoder
{
    Q_OBJECT
public:
    static const int MTU_SIZE = 1412;

    struct Config
    {
        bool computeSignature = false;
        bool useAbsoluteTimestamp = false;
        bool keepOriginalTimestamps = false;
        DecoderConfig decoderConfig;
    };

public:
    QnFfmpegTranscoder(const Config& config, nx::metrics::Storage* metrics);
    ~QnFfmpegTranscoder();

    virtual int setContainer(const QString& value) override;
    void setFormatOption(const QString& option, const QString& value);
    // Force to use this source resolution, instead of max stream resolution from resource streams
    void setSourceResolution(const QSize& resolution);
    bool isCodecSupported(AVCodecID id) const;

    AVCodecParameters* getVideoCodecParameters() const;
    AVCodecParameters* getAudioCodecParameters() const;
    AVFormatContext* getFormatContext() const { return m_formatCtx; }

    virtual int open(const QnConstCompressedVideoDataPtr& video, const QnConstCompressedAudioDataPtr& audio) override;

    //!Implementation of QnTranscoder::addTag
    virtual bool addTag( const QString& name, const QString& value ) override;
    void setInMiddleOfStream(bool value) { m_inMiddleOfStream = value; }
    bool inMiddleOfStream() const { return m_inMiddleOfStream; }
    void setStartTimeOffset(qint64 value) { m_startTimeOffset = value; }

    QByteArray getSignature(QnLicensePool* licensePool, const QnUuid& serverId);

    struct PacketTimestamp
    {
        uint64_t ntpTimestamp = 0;
        uint32_t rtpTimestamp = 0;
    };
    PacketTimestamp getLastPacketTimestamp() const { return m_lastPacketTimestamp; }
    void setRtpMtu(int mtu) { m_rtpMtu = mtu; }
    void setSeeking() { m_isSeeking = true; };

protected:
    virtual int transcodePacketInternal(const QnConstAbstractMediaDataPtr& media) override;
    virtual int finalizeInternal(nx::utils::ByteArray* const result) override;

private:
    //friend qint32 ffmpegWritePacket(void *opaque, quint8* buf, int size);
    AVIOContext* createFfmpegIOContext();
    void closeFfmpegContext();
    int muxPacket(const QnConstAbstractMediaDataPtr& packet);
    AVPixelFormat getPixelFormatJpeg(const QnConstCompressedVideoDataPtr& video);
    bool handleSeek(const QnConstAbstractMediaDataPtr& packet);

private:
    Config m_config;
    QSize m_sourceResolution;
    MediaSigner m_mediaSigner;
    AVCodecParameters* m_videoCodecParameters = nullptr;
    AVCodecParameters* m_audioCodecParameters = nullptr;
    int m_videoBitrate = 0;
    AVFormatContext* m_formatCtx = nullptr;

    QString m_container;
    qint64 m_baseTime = AV_NOPTS_VALUE;
    PacketTimestamp m_lastPacketTimestamp;
    bool m_inMiddleOfStream = false;
    qint64 m_startTimeOffset = 0;
    int m_rtpMtu = MTU_SIZE;
    bool m_isSeeking = false;
};
