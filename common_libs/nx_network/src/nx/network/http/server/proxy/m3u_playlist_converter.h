#pragma once

#include <nx/network/socket_common.h>

#include "message_body_converter.h"

namespace nx {
namespace network {
namespace http {
namespace server {
namespace proxy {

class NX_NETWORK_API M3uPlaylistConverter:
    public AbstractMessageBodyConverter
{
public:
    M3uPlaylistConverter(
        const AbstractUrlRewriter& urlRewriter,
        const SocketAddress& proxyEndpoint,
        const nx::String& targetHost);

    virtual nx::network::http::BufferType convert(nx::network::http::BufferType originalBody) override;

private:
    const AbstractUrlRewriter& m_urlRewriter;
    const SocketAddress m_proxyEndpoint;
    const nx::String m_targetHost;
};

} // namespace proxy
} // namespace server
} // namespace nx
} // namespace network
} // namespace http
