// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/http/server/proxy/message_body_converter.h>

namespace nx {
namespace cloud {
namespace gateway {

class NX_VMS_GATEWAY_API UrlRewriter:
    public nx::network::http::server::proxy::AbstractUrlRewriter
{
public:
    virtual nx::utils::Url originalResourceUrlToProxyUrl(
        const nx::utils::Url& originalResourceUrl,
        const nx::utils::Url& proxyHostUrl,
        const std::string& targetHost) const override;
};

} // namespace gateway
} // namespace cloud
} // namespace nx
