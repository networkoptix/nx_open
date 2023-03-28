// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <deque>
#include <memory>
#include <vector>

#include <nx/media/abstract_video_decoder.h>
#include <nx/media/video_data_packet.h>

namespace nx::media::nvidia {

class Renderer;
class NvidiaVideoFrame;
struct NvidiaVideoDecoderImpl;

class NvidiaVideoDecoder: public std::enable_shared_from_this<NvidiaVideoDecoder>
{
public:
    NvidiaVideoDecoder();
    ~NvidiaVideoDecoder();
    bool decode(const QnConstCompressedVideoDataPtr& frame);

    std::unique_ptr<NvidiaVideoFrame> getFrame();
    void releaseFrame(uint8_t* frame);

    static bool isCompatible(
        const QnConstCompressedVideoDataPtr& frame, AVCodecID codec, int width, int height);

    static bool isAvailable();

    Renderer& getRenderer();

    void pushContext();
    void popContext();

private:
    bool initialize(const QnConstCompressedVideoDataPtr& frame);

private:
    std::unique_ptr<NvidiaVideoDecoderImpl> m_impl;
};

} // namespace nx::media::nvidia
