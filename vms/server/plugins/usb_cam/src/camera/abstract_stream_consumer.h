#pragma once

#include "ffmpeg/packet.h"
#include "ffmpeg/frame.h"

namespace nx {
namespace usb_cam {

class AbstractStreamConsumer
{
public:
    virtual ~AbstractStreamConsumer() = default;
    virtual void flush() = 0;
};

class AbstractPacketConsumer: public AbstractStreamConsumer
{
public:
    virtual void givePacket(const std::shared_ptr<ffmpeg::Packet>& packet) = 0;
};

class AbstractFrameConsumer: public AbstractStreamConsumer
{
public:
    virtual void giveFrame(const std::shared_ptr<ffmpeg::Frame>& frame) = 0;
};

} // namespace usb_cam
} // namespace nx
