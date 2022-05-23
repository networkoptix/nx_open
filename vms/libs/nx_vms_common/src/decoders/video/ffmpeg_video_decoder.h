// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtGui/QImage>

#include "abstract_video_decoder.h"
#include "nx/streaming/video_data_packet.h"
#include <deque>

#include "utils/media/ffmpeg_helper.h"

class FrameTypeExtractor;
struct AVCodec;
struct AVCodecContext;
struct AVFrame;
struct MpegEncContext;

namespace nx::metrics { struct Storage; }

/**
 * Client of this class is responsible for encoded data buffer to meet ffmpeg
 * restrictions (see the comment for decoding functions for details).
 */
class NX_VMS_COMMON_API QnFfmpegVideoDecoder: public QnAbstractVideoDecoder
{
public:
    /*!
        \param swDecoderCount Atomically incremented in constructor and atomically decremented in destructor
    */
    QnFfmpegVideoDecoder(
        const DecoderConfig& config,
        nx::metrics::Storage* metrics,
        const QnConstCompressedVideoDataPtr& data);
    QnFfmpegVideoDecoder(const QnFfmpegVideoDecoder&) = delete;
    QnFfmpegVideoDecoder& operator=(const QnFfmpegVideoDecoder&) = delete;
    ~QnFfmpegVideoDecoder();
    bool decode(
        const QnConstCompressedVideoDataPtr& data,
        CLVideoDecoderOutputPtr* const outFrame) override;

    virtual void setLightCpuMode(DecodeMode val) override;

    AVCodecContext* getContext() const;
    MemoryType targetMemoryType() const override;
    virtual int getWidth() const override { return m_context->width;  }
    virtual int getHeight() const override { return m_context->height; }
    double getSampleAspectRatio() const override;
    void determineOptimalThreadType(const QnConstCompressedVideoDataPtr& data);
    void setMultiThreadDecodePolicy(MultiThreadDecodePolicy mtDecodingPolicy) override;
    virtual bool resetDecoder(const QnConstCompressedVideoDataPtr& data) override;
    int getLastDecodeResult() const override { return m_lastDecodeResult; }

private:
    static AVCodec* findCodec(AVCodecID codecId);

    bool openDecoder(const QnConstCompressedVideoDataPtr& data);
    bool initFFmpegDecoder();
    void processNewResolutionIfChanged(const QnConstCompressedVideoDataPtr& data, int width, int height);
    int decodeVideo(
        AVCodecContext *avctx,
        AVFrame *picture,
        int *got_picture_ptr,
        const AVPacket *avpkt);
    void setMultiThreadDecoding(bool value);

private:
    AVCodec* m_codec = nullptr;
    AVCodecContext *m_context;

    AVCodecID m_codecId;
    QnAbstractVideoDecoder::DecodeMode m_decodeMode;
    QnAbstractVideoDecoder::DecodeMode m_newDecodeMode;

    unsigned int m_lightModeFrameCounter;
    std::unique_ptr<FrameTypeExtractor> m_frameTypeExtractor;

    int m_currentWidth;
    int m_currentHeight;
    bool m_checkH264ResolutionChange;

    int m_forceSliceDecoding;
    mutable double m_prevSampleAspectRatio;
    qint64 m_prevTimestamp;
    qint64 m_prevFrameDuration = 0;
    bool m_spsFound;
    MultiThreadDecodePolicy m_mtDecodingPolicy;
    bool m_useMtDecoding;
    bool m_needRecreate;
    nx::metrics::Storage* m_metrics = nullptr;
    int m_lastDecodeResult = 0;

    QnAbstractMediaData::MediaFlags m_lastFlags {};
    int m_lastChannelNumber = 0;
    const DecoderConfig m_config;
};
