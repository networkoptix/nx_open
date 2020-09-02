#pragma once

#include <decoders/video/abstract_video_decoder.h>
#include <nx/streaming/video_data_packet.h>

namespace nx::media::quick_sync {
    class QuickSyncVideoDecoderImpl;
}

class QuickSyncVideoDecoderOldPlayer: public QnAbstractVideoDecoder
{
public:
    virtual bool decode(const QnConstCompressedVideoDataPtr& data, CLVideoDecoderOutputPtr* const outFrame) override;

    virtual void resetDecoder(const QnConstCompressedVideoDataPtr& data) override;

    virtual int getWidth() const override;
    virtual int getHeight() const override;
    virtual AVPixelFormat GetPixelFormat() const override;
    virtual MemoryType targetMemoryType() const override;
    virtual double getSampleAspectRatio() const override;

    virtual void setLightCpuMode(DecodeMode val) override;
    virtual void setMultiThreadDecodePolicy(MultiThreadDecodePolicy mtDecodingPolicy) override;
    virtual int getLastDecodeResult() const override { return m_lastStatus; };

    static bool isSupported(const QnConstCompressedVideoDataPtr& data);
    virtual void resetDecoder() override;
private:
    std::shared_ptr<nx::media::quick_sync::QuickSyncVideoDecoderImpl> m_impl;
    QSize m_resolution;
    int m_lastStatus = 0;
    AVCodecID m_codecId = AV_CODEC_ID_NONE;
};
