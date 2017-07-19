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

    void prepareMessage(
        const nx::Buffer& payload,
        FrameType type,
        nx::Buffer* outBuffer);

    void prepareFrame(
        const nx::Buffer& payload,
        FrameType type,
        bool fin,
        nx::Buffer* outBuffer);

    int prepareFrame(
        const char* payload, int payloadLen,
        FrameType type, bool fin, char* out, int outLen);

private:
    void setMasked(bool masked, unsigned mask = 0);

private:
    bool m_masked;
    unsigned m_mask;
};


} // namespace websocket
} // namespace network
} // namespace nx