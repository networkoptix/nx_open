#pragma once

#include "decoders/video/abstract_video_decoder.h"
#include "nx/streaming/video_data_packet.h"

namespace nx::media {
    class QuickSyncVideoDecoderImpl;
}

class QuickSyncVideoDecoder: public QnAbstractVideoDecoder
{
public:
    QuickSyncVideoDecoder();
    virtual bool decode( const QnConstCompressedVideoDataPtr& data, QSharedPointer<CLVideoDecoderOutput>* const outFrame) override;

    virtual const AVFrame* lastFrame() const override;
    virtual void resetDecoder(const QnConstCompressedVideoDataPtr& data) override;

    virtual int getWidth() const override;
    virtual int getHeight() const override;
    virtual AVPixelFormat GetPixelFormat() const override;
    virtual MemoryType targetMemoryType() const override;
    virtual double getSampleAspectRatio() const override;

    virtual void setSpeed(float newValue) override;
    virtual void setLightCpuMode(DecodeMode val) override;
    virtual void setMultiThreadDecodePolicy(MultiThreadDecodePolicy mtDecodingPolicy) override;

private:
    std::unique_ptr<nx::media::QuickSyncVideoDecoderImpl> m_impl;
    QSize m_resulution;
};
