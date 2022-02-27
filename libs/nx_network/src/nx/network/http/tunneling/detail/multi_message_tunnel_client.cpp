// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "multi_message_tunnel_client.h"

namespace nx::network::http::tunneling::detail {

MultiMessageClient::MultiMessageClient(
    const nx::utils::Url& baseTunnelUrl,
    ClientFeedbackFunction clientFeedbackFunction)
    :
    base_type(baseTunnelUrl, std::move(clientFeedbackFunction))
{
}

void MultiMessageClient::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);
    // TODO
}

void MultiMessageClient::setTimeout(std::optional<std::chrono::milliseconds> /*timeout*/)
{
    // TODO
}

void MultiMessageClient::openTunnel(
    OpenTunnelCompletionHandler /*completionHandler*/)
{
    // TODO
}

const Response& MultiMessageClient::response() const
{
    return m_response;
}

void MultiMessageClient::stopWhileInAioThread()
{
    base_type::stopWhileInAioThread();
    // TODO
}

} // namespace nx::network::http::tunneling::detail
