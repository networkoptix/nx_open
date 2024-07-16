// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>
#include <memory>

#include <QtCore/QCoreApplication>
#include <QtCore/QElapsedTimer>

#include <decoders/video/ffmpeg_video_decoder.h>
#include <nx/core/transcoding/filters/filter_chain.h>
#include <utils/media/frame_info.h>

#include "transcoder.h"

namespace nx::metrics { struct Storage; }

NX_VMS_COMMON_API AVCodecID findVideoEncoder(const QString& codecName);

class NX_VMS_COMMON_API QnFfmpegVideoTranscoder: public QnVideoTranscoder
{
    Q_DECLARE_TR_FUNCTIONS(QnFfmpegVideoTranscoder)
public:
    QnFfmpegVideoTranscoder(const DecoderConfig& config, nx::metrics::Storage* metrics, AVCodecID codecId);
    ~QnFfmpegVideoTranscoder();

    QnFfmpegVideoTranscoder(const QnFfmpegVideoTranscoder&) = delete;
    QnFfmpegVideoTranscoder& operator=(const QnFfmpegVideoTranscoder&) = delete;

    // Should call before open
    void setFilterChain(const nx::core::transcoding::FilterChain& filters);

    virtual int transcodePacket(const QnConstAbstractMediaDataPtr& media, QnAbstractMediaDataPtr* const result) override;
    virtual bool open(const QnConstCompressedVideoDataPtr& video) override;
    void close();
    AVCodecContext* getCodecContext();

    /* Allow multithread transcoding */
    void setUseMultiThreadEncode(bool value);
    void setUseMultiThreadDecode(bool value);
    void setUseRealTimeOptimization(bool value);
    void setFixedFrameRate(int value);
    //!Returns picture size (in pixels) of output video stream
    QSize getOutputResolution() const;
    // It used to skip some video frames inside GOP when transcoding is used
    void setPreciseStartPosition(int64_t startTimeUs);

    void setOutputResolutionLimit(const QSize& resolution);
    // Force to use this source resolution, instead of max stream resolution from resource streams
    void setSourceResolution(const QSize& resolution);
    // Do not rewrite a "strange" timestamps, e.g. in case with seek.
    void setKeepOriginalTimestamps(bool value);

private:
    int transcodePacketImpl(const QnConstCompressedVideoDataPtr& video, QnAbstractMediaDataPtr* const result);
    bool prepareFilters(AVCodecID dstCodec, const QnConstCompressedVideoDataPtr& video);
    std::pair<uint32_t, QnFfmpegVideoDecoder*> getDecoder(
        const QnConstCompressedVideoDataPtr& video);

private:
    DecoderConfig m_config;

    // It used to skip some video frames inside GOP when transcoding is used
    int64_t m_startTimeUs = 0;

    uint32_t m_lastFlushedDecoder = 0;
    std::map<uint32_t, std::unique_ptr<QnFfmpegVideoDecoder>> m_videoDecoders;
    nx::core::transcoding::FilterChain m_filters;
    QSize m_outputResolutionLimit;
    QSize m_targetResolution;
    QSize m_sourceResolution;

    AVCodecContext* m_encoderCtx;

    int m_lastSrcWidth[CL_MAX_CHANNELS];
    int m_lastSrcHeight[CL_MAX_CHANNELS];

    bool m_useMultiThreadEncode;

    QElapsedTimer m_encodeTimer;
    qint64 m_lastEncodedTime;
    qint64 m_averageCodingTimePerFrame;
    qint64 m_averageVideoTimePerFrame;
    qint64 m_droppedFrames;

    bool m_useRealTimeOptimization;
    bool m_keepOriginalTimestamps = false;
    CodecParametersConstPtr m_ctxPtr;
    nx::metrics::Storage* m_metrics = nullptr;
    std::map<qint64, qint64> m_frameNumToPts;
    int m_fixedFrameRate = 0;
    std::optional<int64_t> m_lastEncodedPts;
};

typedef QSharedPointer<QnFfmpegVideoTranscoder> QnFfmpegVideoTranscoderPtr;
