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
    QnFfmpegVideoTranscoder(nx::metrics::Storage* metrics, AVCodecID codecId);
    ~QnFfmpegVideoTranscoder();

    virtual int transcodePacket(const QnConstAbstractMediaDataPtr& media, QnAbstractMediaDataPtr* const result) override;
    virtual bool open(const QnConstCompressedVideoDataPtr& video) override;
    void close();
    AVCodecContext* getCodecContext();

    /* Allow multithread transcoding */
    void setMTMode(bool value);
    void setUseRealTimeOptimization(bool value);
    virtual void setFilterList(QList<QnAbstractImageFilterPtr> filterList) override;

private:
    int transcodePacketImpl(const QnConstCompressedVideoDataPtr& video, QnAbstractMediaDataPtr* const result);

private:
    QVector<QnFfmpegVideoDecoder*> m_videoDecoders;
    CLVideoDecoderOutputPtr m_decodedVideoFrame;

    quint8* m_videoEncodingBuffer;
    AVCodecContext* m_encoderCtx;

    int m_lastSrcWidth[CL_MAX_CHANNELS];
    int m_lastSrcHeight[CL_MAX_CHANNELS];

    qint64 m_firstEncodedPts;
    bool m_mtMode;

    QElapsedTimer m_encodeTimer;
    qint64 m_lastEncodedTime;
    qint64 m_averageCodingTimePerFrame;
    qint64 m_averageVideoTimePerFrame;
    qint64 m_encodedFrames;
    qint64 m_droppedFrames;

    bool m_useRealTimeOptimization;
    AVPacket* m_outPacket;
    QnConstMediaContextPtr m_ctxPtr;
    nx::metrics::Storage* m_metrics = nullptr;
};

typedef QSharedPointer<QnFfmpegVideoTranscoder> QnFfmpegVideoTranscoderPtr;

#endif // ENABLE_DATA_PROVIDERS

