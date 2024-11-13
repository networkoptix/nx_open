// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/media/ffmpeg/abstract_video_decoder.h>
#include <nx/media/ffmpeg/av_options.h>
#include <nx/media/ffmpeg/frame_info.h>
#include <nx/media/video_data_packet.h>

struct AVCodecContext;
struct AVHWDeviceContext;
namespace nx::metric { struct Storage; }
namespace nx::media::ffmpeg {

class NX_MEDIA_CORE_API HwVideoDecoder : public QnAbstractVideoDecoder
{
public:
    using InitFunc = std::function<bool(AVHWDeviceContext*)>;

    HwVideoDecoder(
        AVHWDeviceType type,
        nx::metric::Storage* metrics,
        std::unique_ptr<AvOptions> options = {},
        InitFunc initFunc = {});
    virtual ~HwVideoDecoder();

    static bool isCompatible(const AVCodecID codec, const QSize& resolution, bool allowOverlay);
    static bool isAvailable();
    static QSize maxResolution(const AVCodecID codec);

    // QnAbstractVideoDecoder impl
    bool decode(
        const QnConstCompressedVideoDataPtr& data,
        CLVideoDecoderOutputPtr* const outFrame) override;

    bool hardwareDecoder() const override {return true; }
    void setLightCpuMode(DecodeMode) override {}
    void setMultiThreadDecodePolicy(MultiThreadDecodePolicy) override {}
    int getWidth() const override;
    int getHeight() const override;
    bool resetDecoder(const QnConstCompressedVideoDataPtr& data) override;
    int getLastDecodeResult() const override;
    void setGreyOnlyMode(bool value) override;

    AVPixelFormat getPixelFormat() { return m_targetPixelFormat; }

    int64_t frameNum() const;

private:
    bool initialize(const QnConstCompressedVideoDataPtr& frame);
    bool initializeHardware(const QnConstCompressedVideoDataPtr& frame);
    bool initializeSoftware(const QnConstCompressedVideoDataPtr& frame);

private:
    AVHWDeviceType m_type;
    InitFunc m_initFunc;
    AVCodecContext* m_decoderContext = nullptr;
    AVBufferRef* m_hwDeviceContext = nullptr;
    AVPixelFormat m_targetPixelFormat = AV_PIX_FMT_NONE;
    int m_lastDecodeResult = 0;
    std::unique_ptr<AvOptions> m_options = nullptr;
    nx::metric::Storage* m_metrics = nullptr;
    qint64 m_lastPts = AV_NOPTS_VALUE;
    quint32 m_lastChannel = 0;
};

} // namespace nx::media::ffmpeg
