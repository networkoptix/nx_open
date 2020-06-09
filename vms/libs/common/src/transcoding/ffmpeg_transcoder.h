#pragma once

#ifdef ENABLE_DATA_PROVIDERS

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include <nx/utils/cryptographic_hash.h>
#include "transcoder.h"
#include "utils/media/frame_info.h"
#include "decoders/video/ffmpeg_video_decoder.h"
#include "decoders/video/ffmpeg_video_decoder.h"
#include <export/signer.h>

class QnLicensePool;

class QnFfmpegTranscoder: public QnTranscoder
{
    Q_OBJECT;

public:
    static const int MTU_SIZE = 1412;

    struct Config
    {
        bool computeSignature = false;
        DecoderConfig decoderConfig;
    };

public:
    QnFfmpegTranscoder(const Config& config, nx::metrics::Storage* metrics);
    ~QnFfmpegTranscoder();

    int setContainer(const QString& value);
    void setFormatOption(const QString& option, const QString& value);
    // Force to use this source resolution, instead of max stream resolution from resource streams
    void setSourceResolution(const QSize& resolution);
    bool isCodecSupported(AVCodecID id) const;

    AVCodecContext* getVideoCodecContext() const;
    AVCodecContext* getAudioCodecContext() const;
    AVFormatContext* getFormatContext() const { return m_formatCtx; }

    virtual int open(const QnConstCompressedVideoDataPtr& video, const QnConstCompressedAudioDataPtr& audio) override;

    //!Implementation of QnTranscoder::addTag
    virtual bool addTag( const QString& name, const QString& value ) override;
    void setInMiddleOfStream(bool value) { m_inMiddleOfStream = value; }
    bool inMiddleOfStream() const { return m_inMiddleOfStream; }
    void setStartTimeOffset(qint64 value) { m_startTimeOffset = value; }

    QByteArray getSignature(QnLicensePool* licensePool);

    struct PacketTimestamp
    {
        uint64_t ntpTimestamp = 0;
        uint32_t rtpTimestamp = 0;
    };
    PacketTimestamp getLastPacketTimestamp() { return m_lastPacketTimestamp; }

protected:
    virtual int transcodePacketInternal(const QnConstAbstractMediaDataPtr& media, QnByteArray* const result) override;
    virtual int finalizeInternal(QnByteArray* const result) override;

private:
    //friend qint32 ffmpegWritePacket(void *opaque, quint8* buf, int size);
    AVIOContext* createFfmpegIOContext();
    void closeFfmpegContext();
    int muxPacket(const QnConstAbstractMediaDataPtr& packet);
    AVPixelFormat getPixelFormatJpeg(const QnConstCompressedVideoDataPtr& video);

private:
    Config m_config;
    QSize m_sourceResolution;
    MediaSigner m_mediaSigner;
    AVCodecContext* m_videoEncoderCodecCtx;
    AVCodecContext* m_audioEncoderCodecCtx;
    int m_videoBitrate;
    AVFormatContext* m_formatCtx;
    QString m_lastErrMessage;

    QString m_container;
    qint64 m_baseTime;
    PacketTimestamp m_lastPacketTimestamp;
    bool m_inMiddleOfStream;
    qint64 m_startTimeOffset;
};

#endif // ENABLE_DATA_PROVIDERS
