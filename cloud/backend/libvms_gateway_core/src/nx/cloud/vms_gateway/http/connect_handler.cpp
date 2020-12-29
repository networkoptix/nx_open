#include "connect_handler.h"

#include <nx/network/aio/async_channel_bridge.h>
#include <nx/network/socket_global.h>
#include <nx/network/http/buffer_source.h>
#include <nx/network/address_resolver.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>

#include "../settings.h"

namespace nx {
namespace cloud {
namespace gateway {

ConnectHandler::ConnectHandler(const conf::Settings& settings,
    TunnelCreatedHandler tunnelCreatedHandler):
    m_settings(settings),
    m_tunnelCreatedHandler(std::move(tunnelCreatedHandler))
{
}

void ConnectHandler::processRequest(
    nx::network::http::RequestContext requestContext,
    nx::network::http::RequestProcessedHandler completionHandler)
{
    const auto& request = requestContext.request;
    auto connection = requestContext.connection;

    // NOTE: this validation should be done somewhere in more general place
    if (!request.requestLine.url.isValid())
        return completionHandler(network::http::StatusCode::badRequest);
    network::SocketAddress targetAddress(request.requestLine.url.host(),
        static_cast<quint16>(request.requestLine.url.port()));
    if (!network::SocketGlobals::addressResolver()
            .isCloudHostName(targetAddress.address.toString()))
    {
        // No cloud address means direct IP.
        if (!m_settings.cloudConnect().allowIpTarget)
            return completionHandler(nx::network::http::StatusCode::forbidden);
        if (targetAddress.port == 0)
            targetAddress.port = static_cast<quint16>(m_settings.http().proxyTargetPort);
    }

    m_targetSocket = nx::network::SocketFactory::createStreamSocket();
    m_targetSocket->bindToAioThread(connection->getAioThread());
    if (!m_targetSocket->setNonBlockingMode(true) ||
        !m_targetSocket->setRecvTimeout(m_settings.tcp().recvTimeout) ||
        !m_targetSocket->setSendTimeout(m_settings.tcp().sendTimeout))
    {
        NX_INFO(this, "Failed to set socket options. %1", SystemError::getLastOSErrorText());
        return completionHandler(nx::network::http::StatusCode::internalServerError);
    }

    m_request = std::move(request);
    m_connection = connection;
    connect(targetAddress, std::move(completionHandler));
}

void ConnectHandler::connect(
    const network::SocketAddress& address,
    network::http::RequestProcessedHandler completionHandler)
{
    NX_DEBUG(this, "Connecting to '%1', socket[%2] -> socket[%3].",
        address, m_connection->socket().get(), m_targetSocket.get());

    m_targetSocket->connectAsync(
        address,
        [this, completionHandler = std::move(completionHandler)](SystemError::ErrorCode error)
        {
            if (error != SystemError::noError)
            {
                NX_INFO(this, "Socket %1 connecting error: %2.",
                    m_targetSocket.get(), SystemError::toString(error));
                return completionHandler(nx::network::http::StatusCode::serviceUnavailable);
            }
            NX_VERBOSE(this, "Successfully connected to %1", m_targetSocket->getForeignAddress());

            // TODO: check when is called onResponseSendHandler, is it possible to skip some data
            //      between sending response and start of tunnel
            network::http::RequestResult result(network::http::StatusCode::ok);
            result.connectionEvents.onResponseHasBeenSent =
                [targetSocket = std::move(m_targetSocket),
                    tunnelCreatedHandler = std::move(m_tunnelCreatedHandler)](
                    network::http::HttpServerConnection *connection) mutable
                {
                    auto clientSocket = connection->takeSocket();
                    clientSocket->cancelIOSync(network::aio::etNone);

                    tunnelCreatedHandler(network::aio::makeAsyncChannelBridge(
                        std::move(clientSocket), std::move(targetSocket)));
                };
            return completionHandler(std::move(result));
        });
}

} // namespace gateway
} // namespace cloud
} // namespace nx
