#pragma once

#include <nx/network/socket_common.h>

#include "message_body_converter.h"

namespace nx {
namespace cloud {
namespace gateway {

class NX_VMS_GATEWAY_API M3uPlaylistConverter:
    public AbstractMessageBodyConverter
{
public:
    M3uPlaylistConverter(
        const SocketAddress& proxyEndpoint,
        const nx::String& targetHost);

    virtual nx_http::BufferType convert(nx_http::BufferType originalBody) override;

private:
    const SocketAddress m_proxyEndpoint;
    const nx::String m_targetHost;
};

} // namespace gateway
} // namespace cloud
} // namespace nx
