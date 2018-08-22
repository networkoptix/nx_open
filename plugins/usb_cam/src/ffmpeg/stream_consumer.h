#pragma once

#include "packet.h"
#include "frame.h"

namespace nx {
namespace ffmpeg {

class StreamConsumer
{
public:
    // virtual int fps() const = 0;
    // virtual void resolution(int *width, int *height) const = 0;
    // virtual int bitrate() const = 0;
    virtual void flush() = 0;
};

class PacketConsumer : public StreamConsumer
{
public:
    virtual void givePacket(const std::shared_ptr<Packet>& packet) = 0;
};

class FrameConsumer : public StreamConsumer
{
public:
    virtual void giveFrame(const std::shared_ptr<Frame>& frame) = 0;
};

class VideoConsumer
{
public:
    virtual int fps() const = 0;
    virtual void resolution(int *width, int *height) const = 0;
    virtual int bitrate() const = 0;
};

} // namespace ffmpeg
} // namespace nx