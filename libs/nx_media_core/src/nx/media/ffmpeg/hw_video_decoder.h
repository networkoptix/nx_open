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
        const std::string& device = {},
        std::unique_ptr<AvOptions> options = {},
        InitFunc initFunc = {});
    virtual ~HwVideoDecoder();

    static bool isCompatible(
        const AVCodecID codec,
        const QSize& resolution,
        bool allowHardwareAcceleration);
    static bool isAvailable();
    static QSize maxResolution(const AVCodecID codec);

    // QnAbstractVideoDecoder impl
    bool decode(
        const QnConstCompressedVideoDataPtr& data,
        CLVideoDecoderOutputPtr* const outFrame) override;

    bool hardwareDecoder() const override {return m_hardwareMode; }
    void setLightCpuMode(DecodeMode) override {}
    void setMultiThreadDecodePolicy(MultiThreadDecodePolicy policy) override;
    int getWidth() const override;
    int getHeight() const override;
    bool resetDecoder(const QnConstCompressedVideoDataPtr& data) override;
    int getLastDecodeResult() const override;
    void setGreyOnlyMode(bool value) override;

    AVPixelFormat getPixelFormat() { return m_targetPixelFormat; }

    // To support AbstractVideoDecoder impl
    bool sendPacket(const QnConstCompressedVideoDataPtr& packet);
    bool receiveFrame(CLVideoDecoderOutputPtr* const outFrame);
    int currentFrameNumber() const;

private:
    bool initialize(const QnConstCompressedVideoDataPtr& frame);
    bool initializeHardware(const QnConstCompressedVideoDataPtr& frame);
    bool initializeSoftware(const QnConstCompressedVideoDataPtr& frame);

private:
    AVHWDeviceType m_type;
    InitFunc m_initFunc;
    bool m_hardwareMode = true;
    AVCodecContext* m_decoderContext = nullptr;
    AVBufferRef* m_hwDeviceContext = nullptr;
    AVPixelFormat m_targetPixelFormat = AV_PIX_FMT_NONE;
    int m_lastDecodeResult = 0;
    std::string m_device;
    std::unique_ptr<AvOptions> m_options = nullptr;
    nx::metric::Storage* m_metrics = nullptr;
    uint32_t m_channel = 0;
    QnAbstractMediaData::MediaFlags m_flags {};
    MultiThreadDecodePolicy m_mtDecodingPolicy = MultiThreadDecodePolicy::enabled;
};

} // namespace nx::media::ffmpeg
