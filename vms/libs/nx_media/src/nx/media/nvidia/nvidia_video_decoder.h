// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>
#include <memory>
#include <deque>

#include <nx/streaming/video_data_packet.h>
#include <nx/media/abstract_video_decoder.h>


class NvidiaVideoFrame;

namespace nx::media::nvidia {

namespace linux{
    class Renderer;
}

struct NvidiaVideoDecoderImpl;

class NvidiaVideoDecoder: public std::enable_shared_from_this<NvidiaVideoDecoder>
{
public:
    NvidiaVideoDecoder();
    ~NvidiaVideoDecoder();
    int decode(const QnConstCompressedVideoDataPtr& frame, nx::QVideoFramePtr* result = nullptr);

    std::unique_ptr<NvidiaVideoFrame> getFrame();

    static bool isCompatible(
        const QnConstCompressedVideoDataPtr& frame, AVCodecID codec, int width, int height);

//    bool scaleFrame(
  //      mfxFrameSurface1* inputSurface, mfxFrameSurface1** outSurface, const QSize& targetSize);

    linux::Renderer& getRenderer();
    void resetDecoder();

private:
    bool initialize(const QnConstCompressedVideoDataPtr& frame);

private:
    std::unique_ptr<NvidiaVideoDecoderImpl> m_impl;
};

} // namespace nx::media::nvidia
