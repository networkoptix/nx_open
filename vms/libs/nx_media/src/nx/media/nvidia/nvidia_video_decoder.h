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

class NX_MEDIA_API NvidiaVideoDecoder: public std::enable_shared_from_this<NvidiaVideoDecoder>
{
public:
    NvidiaVideoDecoder(bool checkMode = false);
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
    // Mode when video will not be decoded, just create a decoder to check compatibility and GPU
    // memory availability.
    bool m_checkMode = false;

    std::unique_ptr<NvidiaVideoDecoderImpl> m_impl;
};

} // namespace nx::media::nvidia
