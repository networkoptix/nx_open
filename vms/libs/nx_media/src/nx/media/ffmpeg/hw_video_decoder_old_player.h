// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/media/ffmpeg/abstract_video_decoder.h>
#include <nx/media/video_data_packet.h>

class QRhi;

namespace nx::media::ffmpeg { class HwVideoDecoder; }

namespace nx::media {

class NX_MEDIA_API HwVideoDecoderOldPlayer: public QnAbstractVideoDecoder
{
public:
    HwVideoDecoderOldPlayer(QRhi* rhi);
    virtual ~HwVideoDecoderOldPlayer();

    virtual bool decode(const QnConstCompressedVideoDataPtr& data, CLVideoDecoderOutputPtr* const outFrame) override;

    virtual bool resetDecoder(const QnConstCompressedVideoDataPtr& data) override;

    virtual int getWidth() const override;
    virtual int getHeight() const override;
    virtual bool hardwareDecoder() const override;
    virtual double getSampleAspectRatio() const override;

    virtual void setLightCpuMode(DecodeMode val) override;
    virtual void setMultiThreadDecodePolicy(MultiThreadDecodePolicy mtDecodingPolicy) override;
    virtual int getLastDecodeResult() const override { return m_lastStatus; };

    static bool isSupported(const QnConstCompressedVideoDataPtr& data);
    static int instanceCount();

private:
    QRhi* m_rhi = nullptr;
    std::shared_ptr<nx::media::ffmpeg::HwVideoDecoder> m_impl;
    QSize m_resolution;
    int m_lastStatus = 0;
    QnAbstractMediaData::MediaFlags m_lastFlags {};
    static int m_instanceCount;
    bool m_isHW = true;
};

} // namespace nx::media
