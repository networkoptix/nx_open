// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <cstdint>
#include <deque>
#include <string_view>

#include <QtGui/QImage>

#include <nx/media/ffmpeg/abstract_video_decoder.h>
#include <nx/media/ffmpeg/shared_memory_frame_allocator.h>
#include <nx/media/ffmpeg_helper.h>
#include <nx/media/video_data_packet.h>

class CLVideoDecoderOutput;
class FrameTypeExtractor;
struct AVCodec;
struct AVCodecContext;
struct AVFrame;
struct MpegEncContext;

namespace nx::metric { struct Storage; }

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
        nx::metric::Storage* metrics,
        const QnConstCompressedVideoDataPtr& data);
    QnFfmpegVideoDecoder(const QnFfmpegVideoDecoder&) = delete;
    QnFfmpegVideoDecoder& operator=(const QnFfmpegVideoDecoder&) = delete;
    ~QnFfmpegVideoDecoder();
    bool decode(
        const QnConstCompressedVideoDataPtr& data,
        CLVideoDecoderOutputPtr* const outFrame) override;

    virtual void setLightCpuMode(DecodeMode val) override;

    AVCodecContext* getContext() const;
    bool hardwareDecoder() const override;
    virtual int getWidth() const override { return m_context->width;  }
    virtual int getHeight() const override { return m_context->height; }
    virtual AVCodecID codec() const override { return m_context->codec_id; }
    double getSampleAspectRatio() const override;
    void setMultiThreadDecodePolicy(MultiThreadDecodePolicy mtDecodingPolicy) override;
    virtual bool resetDecoder(const QnConstCompressedVideoDataPtr& data) override;
    int getLastDecodeResult() const override { return m_lastDecodeResult; }
    void setGreyOnlyMode(bool value) override;

private:
    void determineOptimalThreadType(const QnConstCompressedVideoDataPtr& data);
    bool openDecoder(const QnConstCompressedVideoDataPtr& data);
    bool initFFmpegDecoder();
    void processNewResolutionIfChanged(const QnConstCompressedVideoDataPtr& data, int width, int height);
    bool decodeVideo(const QnConstCompressedVideoDataPtr& data);
    void setMultiThreadDecoding(bool value);

    /** Applies pixel format / aspect ratio / flags / channel bookkeeping shared by every
     * decoded frame this class hands out.
     */
    bool finalizeDecodedFrame(const QnConstCompressedVideoDataPtr& data, CLVideoDecoderOutput* outFrame);

private:
    std::unique_ptr<nx::media::ffmpeg::FfmpegSharedMemoryBufferContext> m_shmBufferContext;
    AVCodecContext *m_context;

    AVCodecID m_codecId = AV_CODEC_ID_NONE;
    QnAbstractVideoDecoder::DecodeMode m_decodeMode;
    QnAbstractVideoDecoder::DecodeMode m_newDecodeMode;

    unsigned int m_lightModeFrameCounter;
    std::unique_ptr<FrameTypeExtractor> m_frameTypeExtractor;

    int m_currentWidth;
    int m_currentHeight;

    int m_forceSliceDecoding;
    mutable double m_prevSampleAspectRatio;
    int64_t m_prevTimestamp;
    int64_t m_prevFrameDuration = 0;
    MultiThreadDecodePolicy m_mtDecodingPolicy;
    bool m_useMtDecoding;
    bool m_needRecreate;
    nx::metric::Storage* m_metrics = nullptr;
    int m_lastDecodeResult = 0;

    QnAbstractMediaData::MediaFlags m_lastFlags {};
    int m_lastChannelNumber = 0;
    const DecoderConfig m_config;
    bool m_opened = false;

    /** Every frame decodeVideo() has retrieved from the decoder but decode() has not yet
     * returned to its caller, oldest first. Normally holds at most one frame; can hold more
     * right after a burst of reordered output, which is then delivered one per decode() call
     * instead of being dropped. */
    std::deque<CLVideoDecoderOutputPtr> m_outputFrames;
};

NX_VMS_COMMON_API CLVideoDecoderOutputPtr transferDecodedFrameToSystemMemory(
    const CLConstVideoDecoderOutputPtr& sourceFrame,
    bool useSharedMemory,
    const nx::media::ffmpeg::FfmpegSharedMemoryAllocatorPtr& sharedMemoryAllocator);

NX_VMS_COMMON_API CLVideoDecoderOutputPtr ensureDecodedFrameInSharedMemory(
    const CLConstVideoDecoderOutputPtr& sourceFrame,
    const nx::media::ffmpeg::FfmpegSharedMemoryAllocatorPtr& sharedMemoryAllocator);

/**
 * Logs broken decoder contract where a frame still has hw_frames_ctx but is not marked
 * as MemoryType::VideoMemory. `context` should identify the caller/checkpoint.
 */
NX_VMS_COMMON_API void logVideoMemoryInvariantViolation(
    const CLConstVideoDecoderOutputPtr& frame,
    std::string_view context);
