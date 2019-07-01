#pragma once

#include "websocket_common_types.h"
#include <nx/network/buffer.h>

namespace nx {
namespace network {
namespace websocket {

class NX_NETWORK_API Serializer
{
public:
    Serializer(bool masked, unsigned mask = 0);

    nx::Buffer prepareMessage(nx::Buffer payload, FrameType type, CompressionType compressionType);

    nx::Buffer prepareFrame(nx::Buffer payload, FrameType type, bool fin, bool first);

private:
    bool m_masked;
    bool m_doCompress = false;
    unsigned m_mask;

    void setMasked(bool masked, unsigned mask = 0);
    int fillHeader(
        char* data, bool fin, bool first, FrameType opCode, int payloadLenType, int payloadLen);
};


} // namespace websocket
} // namespace network
} // namespace nx