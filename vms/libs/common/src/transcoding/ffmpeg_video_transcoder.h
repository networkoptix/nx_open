#pragma once

#ifdef ENABLE_DATA_PROVIDERS

#include <QCoreApplication>
#include <QElapsedTimer>

#include "transcoder.h"

namespace nx { namespace metrics { struct Storage; } }

extern "C"
{
    #include <libswscale/swscale.h>
}

#include "utils/media/frame_info.h"
#include "decoders/video/ffmpeg_video_decoder.h"


AVCodecID findVideoEncoder(const QString& codecName);

class QnFfmpegVideoTranscoder: public QnVideoTranscoder
{
    Q_DECLARE_TR_FUNCTIONS(QnFfmpegVideoTranscoder)
public:
    QnFfmpegVideoTranscoder(const DecoderConfig& config, nx::metrics::Storage* metrics, AVCodecID codecId);
    ~QnFfmpegVideoTranscoder();

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

    void setOutputResolutionLimit(const QSize& resolution);
    // Force to use this source resolution, instead of max stream resolution from resource streams
    void setSourceResolution(const QSize& resolution);

private:
    int transcodePacketImpl(const QnConstCompressedVideoDataPtr& video, QnAbstractMediaDataPtr* const result);
    QSharedPointer<CLVideoDecoderOutput> processFilterChain(
        const QSharedPointer<CLVideoDecoderOutput>& decodedFrame);
    bool prepareFilters(AVCodecID dstCodec, const QnConstCompressedVideoDataPtr& video);

private:
    DecoderConfig m_config;
    QVector<QnFfmpegVideoDecoder*> m_videoDecoders;
    nx::core::transcoding::FilterChain m_filters;
    QSize m_targetResolution;
    QSize m_sourceResolution;
    CLVideoDecoderOutputPtr m_decodedVideoFrame;

    quint8* m_videoEncodingBuffer;
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
    QnConstMediaContextPtr m_ctxPtr;
    nx::metrics::Storage* m_metrics = nullptr;
    std::map<qint64, qint64> m_frameNumToPts;
    int m_fixedFrameRate = 0;
};

typedef QSharedPointer<QnFfmpegVideoTranscoder> QnFfmpegVideoTranscoderPtr;

#endif // ENABLE_DATA_PROVIDERS

