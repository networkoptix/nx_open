// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>

#include <nx/media/ffmpeg/abstract_video_decoder.h>
#include <nx/media/video_data_packet.h>

class QRhi;

namespace nx::media::ffmpeg { class HwVideoDecoder; }

namespace nx::media {

class VideoApiDecoderData;

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
    /** Updates the HW device back-off if the HW device of the current decoder is not checked. */
    void notePendingHwDeviceResult(bool success);

private:
    QRhi* m_rhi = nullptr;
    std::shared_ptr<nx::media::ffmpeg::HwVideoDecoder> m_impl;
    std::shared_ptr<VideoApiDecoderData> m_decoderData;
    QSize m_resolution;
    int m_lastStatus = 0;
    QnAbstractMediaData::MediaFlags m_lastFlags {};
    static int m_instanceCount;
    bool m_isHW = true;

    /** Key of the HW device which was requested for the current decoder and is not checked. */
    std::string m_pendingHwDeviceKey;
    MultiThreadDecodePolicy m_mtDecodingPolicy = MultiThreadDecodePolicy::enabled;
};

} // namespace nx::media
