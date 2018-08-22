#pragma once

#include "ffmpeg/packet.h"
#include "ffmpeg/frame.h"

namespace nx {
namespace usb_cam {

class StreamConsumer
{
public:
    virtual void flush() = 0;
};

class PacketConsumer : public StreamConsumer
{
public:
    virtual void givePacket(const std::shared_ptr<ffmpeg::Packet>& packet) = 0;
};

class FrameConsumer : public StreamConsumer
{
public:
    virtual void giveFrame(const std::shared_ptr<ffmpeg::Frame>& frame) = 0;
};

class VideoConsumer
{
public:
    virtual int fps() const = 0;
    virtual void resolution(int *width, int *height) const = 0;
    virtual int bitrate() const = 0;
};

} // namespace usb_cam
} // namespace nx