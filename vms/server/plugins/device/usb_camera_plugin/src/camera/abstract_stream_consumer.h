#pragma once

#include "ffmpeg/packet.h"
#include "ffmpeg/frame.h"

namespace nx::usb_cam {

class AbstractPacketConsumer
{
public:
    virtual ~AbstractPacketConsumer() = default;
    virtual void flush() = 0;
    virtual void pushPacket(const std::shared_ptr<ffmpeg::Packet>& packet) = 0;
};

} // namespace nx::usb_cam
