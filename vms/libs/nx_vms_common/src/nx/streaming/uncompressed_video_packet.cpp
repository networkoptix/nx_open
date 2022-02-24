// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "uncompressed_video_packet.h"

namespace nx::streaming {

UncompressedVideoPacket::UncompressedVideoPacket(CLConstVideoDecoderOutputPtr uncompressedFrame):
    m_frame(uncompressedFrame)
{
    this->timestamp = m_frame->pkt_dts;
}

CLConstVideoDecoderOutputPtr UncompressedVideoPacket::frame() const
{
    return m_frame;
}

} // namespace nx::streaming
