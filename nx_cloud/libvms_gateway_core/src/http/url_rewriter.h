#pragma once

#include "message_body_converter.h"

namespace nx {
namespace cloud {
namespace gateway {

class NX_VMS_GATEWAY_API UrlRewriter:
    public AbstractUrlRewriter
{
public:
    virtual nx::utils::Url originalResourceUrlToProxyUrl(
        const nx::utils::Url& originalResourceUrl,
        const SocketAddress& proxyEndpoint,
        const nx::String& targetHost) const override;
};

} // namespace gateway
} // namespace cloud
} // namespace nx
