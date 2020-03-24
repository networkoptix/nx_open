#pragma once

#include <decoders/video/abstract_video_decoder.h>
#include <nx/streaming/video_data_packet.h>

namespace nx::media {
    class QuickSyncVideoDecoderImpl;
}

class QuickSyncVideoDecoderOldPlayer: public QnAbstractVideoDecoder
{
public:
    virtual bool decode(const QnConstCompressedVideoDataPtr& data, CLVideoDecoderOutputPtr* const outFrame) override;

    virtual const AVFrame* lastFrame() const override;
    virtual void resetDecoder(const QnConstCompressedVideoDataPtr& data) override;

    virtual int getWidth() const override;
    virtual int getHeight() const override;
    virtual AVPixelFormat GetPixelFormat() const override;
    virtual MemoryType targetMemoryType() const override;
    virtual double getSampleAspectRatio() const override;

    virtual void setLightCpuMode(DecodeMode val) override;
    virtual void setMultiThreadDecodePolicy(MultiThreadDecodePolicy mtDecodingPolicy) override;

    static bool isSupported(AVCodecID codec);

private:
    std::shared_ptr<nx::media::QuickSyncVideoDecoderImpl> m_impl;
    QSize m_resolution;
    AVCodecID m_codecId = AV_CODEC_ID_NONE;
};
