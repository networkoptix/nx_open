#pragma once

#include "websocket_common_types.h"
#include <nx/network/buffer.h>

namespace nx {
namespace network {
namespace websocket {

class NX_NETWORK_API Serializer
{
public:
    Serializer(bool masked, CompressionType compressionType, unsigned mask = 0);

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
    bool m_masked;
    CompressionType m_compressionType;
    unsigned m_mask;

    void setMasked(bool masked, unsigned mask = 0);
    int fillHeader(char* data, bool fin, int opCode, int payloadLenType, int payloadLen);
};


} // namespace websocket
} // namespace network
} // namespace nx