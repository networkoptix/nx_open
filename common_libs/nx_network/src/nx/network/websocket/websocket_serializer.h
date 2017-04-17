#pragma once

#include <nx/network/websocket/websocket_common_types.h>

namespace nx {
namespace network {
namespace websocket {

/** 
 * @return needed output buffer length.
 * @note doesn't write to the output buffer if it's size is not enough.
 */
int NX_NETWORK_API prepareFrame(const char* payload, int payloadLen, FrameType type, 
    bool fin, bool masked, unsigned int mask, char* out, int outLen);

} // namespace websocket
} // namespace network
} // namespace nx