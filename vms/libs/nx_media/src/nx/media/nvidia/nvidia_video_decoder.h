// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>
#include <memory>
#include <deque>

#include <nx/streaming/video_data_packet.h>
#include <nx/media/abstract_video_decoder.h>

namespace nx::media::nvidia {

namespace linux{
    class Renderer;
}

class NvidiaVideoFrame;
struct NvidiaVideoDecoderImpl;

class NvidiaVideoDecoder: public std::enable_shared_from_this<NvidiaVideoDecoder>
{
public:
    NvidiaVideoDecoder();
    ~NvidiaVideoDecoder();
    int decode(const QnConstCompressedVideoDataPtr& frame);

    std::unique_ptr<NvidiaVideoFrame> getFrame();
    void releaseFrame(uint8_t* frame);

    static bool isCompatible(
        const QnConstCompressedVideoDataPtr& frame, AVCodecID codec, int width, int height);

    static int instanceCount() { return m_instanceCount; }

//    bool scaleFrame(
  //      mfxFrameSurface1* inputSurface, mfxFrameSurface1** outSurface, const QSize& targetSize);

    linux::Renderer& getRenderer();

    void pushContext();
    void popContext();

private:
    bool initialize(const QnConstCompressedVideoDataPtr& frame);

private:
    static int m_instanceCount;
    std::unique_ptr<NvidiaVideoDecoderImpl> m_impl;
};

} // namespace nx::media::nvidia
