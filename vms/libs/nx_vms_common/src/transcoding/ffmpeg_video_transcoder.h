// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>
#include <memory>

#include <QtCore/QCoreApplication>
#include <QtCore/QElapsedTimer>

#include <decoders/video/ffmpeg_video_decoder.h>
#include <nx/analytics/caching_metadata_consumer.h>
#include <nx/core/transcoding/filters/filter_chain.h>
#include <nx/media/ffmpeg/frame_info.h>
#include <nx/media/video_data_packet.h>
#include <transcoding/abstract_codec_transcoder.h>
#include <transcoding/transcoding_utils.h>

namespace nx::metric { struct Storage; }

NX_VMS_COMMON_API AVCodecID findVideoEncoder(const QString& codecName);

class NX_VMS_COMMON_API QnFfmpegVideoTranscoder: public AbstractCodecTranscoder
{
public:
    struct TranscodePolicy
    {
        bool useMultiThreadEncode = false;
        bool useRealTimeOptimization = false;
        bool disableTranscoding = false;
        DecoderConfig decoderConfig;
    };

    struct Config: public TranscodePolicy
    {
        AVCodecID targetCodecId = AV_CODEC_ID_NONE;
        QSize outputResolutionLimit;
        // Source resolution, if empty - first video frame will be used (it can be low quality, and
        // in that case result will be transcoded to low quality resolution). So if source stream
        // has mixed high/low video data, set this option to high quality resolution.
        QSize sourceResolution;
        // Set output stream bitrate (bps).
        int bitrate = -1;
        int fixedFrameRate = 0;
        // Set codec-specific params for output stream.
        QnCodecParams::Value params;
        Qn::StreamQuality quality = Qn::StreamQuality::normal;
        // It used to skip some video frames inside GOP when transcoding is used.
        int64_t startTimeUs = 0;
        // Set this to true if stream will mux into RTP.
        bool rtpContatiner = false;
    };

public:
    QnFfmpegVideoTranscoder(const Config& config, nx::metric::Storage* metrics);
    ~QnFfmpegVideoTranscoder();

    QnFfmpegVideoTranscoder(const QnFfmpegVideoTranscoder&) = delete;
    QnFfmpegVideoTranscoder& operator=(const QnFfmpegVideoTranscoder&) = delete;

    // Should call before open
    void setFilterChain(nx::core::transcoding::FilterChainPtr filters);

    virtual int transcodePacket(const QnConstAbstractMediaDataPtr& media, QnAbstractMediaDataPtr* const result) override;
    virtual bool open(const QnConstCompressedVideoDataPtr& video);
    void close();
    AVCodecContext* getCodecContext();
    AVCodecParameters* getCodecParameters();

    //!Returns picture size (in pixels) of output video stream
    QSize getOutputResolution() const;

    // Process metadata, required for the pixelation filter, object info filter and motion filter.
    void processMetadata(const QnConstAbstractCompressedMetadataPtr& metadata);

private:
    int transcodePacketImpl(const QnConstCompressedVideoDataPtr& video, QnAbstractMediaDataPtr* const result);
    bool prepareFilters(AVCodecID dstCodec, const QnConstCompressedVideoDataPtr& video);
    std::pair<uint32_t, QnFfmpegVideoDecoder*> getDecoder(
        const QnConstCompressedVideoDataPtr& video);

private:
    const Config m_config;
    uint32_t m_lastFlushedDecoder = 0;
    std::map<uint32_t, std::unique_ptr<QnFfmpegVideoDecoder>> m_videoDecoders;
    nx::core::transcoding::FilterChainPtr m_filters;
    QSize m_outputResolutionLimit;
    QSize m_targetResolution;
    QSize m_sourceResolution;
    int m_bitrate = -1;

    AVCodecContext* m_encoderCtx = nullptr;
    int m_lastSrcWidth[CL_MAX_CHANNELS];
    int m_lastSrcHeight[CL_MAX_CHANNELS];
    QElapsedTimer m_encodeTimer;
    qint64 m_lastEncodedTime = AV_NOPTS_VALUE;
    qint64 m_averageCodingTimePerFrame = 0;
    qint64 m_averageVideoTimePerFrame = 0;
    qint64 m_droppedFrames = 0;
    CodecParametersConstPtr m_codecParamaters;
    nx::metric::Storage* m_metrics = nullptr;
    std::map<qint64, qint64> m_frameNumToPts;
    std::optional<int64_t> m_lastEncodedPts;
    nx::analytics::CachingMetadataConsumer<QnConstAbstractCompressedMetadataPtr>
        m_metadataObjectsCache;
    nx::analytics::CachingMetadataConsumer<QnConstAbstractCompressedMetadataPtr>
        m_metadataMotionCache;
};

typedef QSharedPointer<QnFfmpegVideoTranscoder> QnFfmpegVideoTranscoderPtr;
