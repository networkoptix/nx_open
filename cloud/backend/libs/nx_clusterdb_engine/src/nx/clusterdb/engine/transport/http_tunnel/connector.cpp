#include "connector.h"

#include <nx/utils/log/log.h>

#include "command_pipeline.h"

namespace nx::clusterdb::engine::transport {

HttpTunnelTransportConnector::HttpTunnelTransportConnector(
    const std::string& systemId,
    const std::string& connectionId,
    const nx::utils::Url& targetUrl)
    :
    m_systemId(systemId),
    m_connectionId(connectionId),
    m_targetUrl(targetUrl),
    m_client(std::make_unique<nx::network::http::tunneling::Client>(targetUrl))
{
    bindToAioThread(getAioThread());
}

void HttpTunnelTransportConnector::bindToAioThread(
    network::aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    if (m_client)
        m_client->bindToAioThread(aioThread);
}

void HttpTunnelTransportConnector::connect(Handler completionHandler)
{
    m_completionHandler = std::move(completionHandler);

    // TODO: #ak Adding auxiliary headers.

    m_client->openTunnel(
        [this](auto... args) { processOpenTunnelResult(std::move(args)...); });
}

void HttpTunnelTransportConnector::stopWhileInAioThread()
{
    m_client.reset();
}

void HttpTunnelTransportConnector::processOpenTunnelResult(
    nx::network::http::tunneling::OpenTunnelResult result)
{
    if (result.sysError != SystemError::noError ||
        !nx::network::http::StatusCode::isSuccessCode(result.httpStatus))
    {
        NX_DEBUG(this, lm("Error connecting to %1. %2, %3")
            .args(m_targetUrl, SystemError::toString(result.sysError),
                nx::network::http::StatusCode::toString(result.httpStatus)));
        return nx::utils::swapAndCall(
            m_completionHandler,
            ConnectResultDescriptor(result.sysError, result.httpStatus),
            nullptr);
    }

    //auto connection = std::make_unique<HttpTunnelTransportConnection>(
    //    m_connectionId,
    //    std::move(result.connection),
    //    0); // TODO: #ak Protocol version.

    nx::utils::swapAndCall(
        m_completionHandler,
        ConnectResultDescriptor(SystemError::notImplemented),
        nullptr);
}

} // namespace nx::clusterdb::engine::transport
