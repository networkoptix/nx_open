// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/socket_common.h>

#include "message_body_converter.h"

namespace nx::network::http::server::proxy {

class NX_NETWORK_API M3uPlaylistConverter:
    public AbstractMessageBodyConverter
{
public:
    M3uPlaylistConverter(
        const AbstractUrlRewriter& urlRewriter,
        const nx::utils::Url& proxyHostUrl,
        const std::string& targetHost);

    virtual nx::Buffer convert(nx::Buffer originalBody) override;

private:
    const AbstractUrlRewriter& m_urlRewriter;
    const nx::utils::Url m_proxyHostUrl;
    const std::string m_targetHost;
};

} // namespace nx::network::http::server::proxy
