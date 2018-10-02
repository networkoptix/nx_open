#include "connect_handler.h"

#include <nx/network/socket_global.h>
#include <nx/network/http/buffer_source.h>
#include <nx/network/address_resolver.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>

#include "settings.h"

namespace nx {
namespace cloud {
namespace gateway {

static const int kDefaultBufferSize = 16 * 1024;


Tunnel::Tunnel(std::unique_ptr<Socket> client, std::unique_ptr<Socket> target,
    TunnelClosedHandler tunnelClosedHandler):
        m_tunnelClosedHandler(std::move(tunnelClosedHandler)),
        m_connectionSocket(std::move(client)),
        m_targetSocket(std::move(target))
{
    m_connectionBuffer.reserve(kDefaultBufferSize);
    m_connectionBuffer.resize(0);

    m_targetBuffer.reserve(kDefaultBufferSize);
    m_targetBuffer.resize(0);
}

void Tunnel::start()
{
    stream(m_connectionSocket.get(), m_targetSocket.get(), &m_connectionBuffer);
    stream(m_targetSocket.get(), m_connectionSocket.get(), &m_targetBuffer);
}

void Tunnel::pleaseStop(nx::utils::MoveOnlyFunc<void ()> completionHandler)
{
    m_connectionSocket->dispatch(
        [this]()
        {
            NX_ASSERT(m_connectionSocket->getAioThread() == m_targetSocket->getAioThread());
            m_connectionSocket.reset();
            m_targetSocket.reset();
        });

    completionHandler();
}

void Tunnel::stream(Socket* source, Socket* target, Buffer* buffer)
{
    source->readSomeAsync(buffer,
        [=](SystemError::ErrorCode result, size_t size)
        {
            if (result != SystemError::noError)
                return socketError(source, result);

            // Tunnel should be closed when one of the peers finishes connection.
            if (size == 0)
                return pleaseStop(
                    [this, handler = std::move(m_tunnelClosedHandler)]()
                    {
                        handler(this);
                    });

            target->sendAsync(*buffer,
                [=](SystemError::ErrorCode result, size_t)
                {
                    if (result != SystemError::noError)
                        return socketError(target, result);

                    stream(source, target, buffer);
                });
        });
}

void Tunnel::socketError(Tunnel::Socket *socket, SystemError::ErrorCode error)
{
    NX_WARNING(this, "Socket %1 returned error: %2. Closing tunnel.",
        socket, SystemError::toString(error));
    return pleaseStop(
        [this, handler = std::move(m_tunnelClosedHandler)]()
        {
            handler(this);
        });
}


ConnectHandler::ConnectHandler(const conf::Settings& settings,
    TunnelCreatedHandler tunnelCreatedHandler, TunnelClosedHandler tunnelClosedHandler):
    m_settings(settings),
    m_tunnelCreatedHandler(std::move(tunnelCreatedHandler)),
    m_tunnelClosedHandler(std::move(tunnelClosedHandler))
{
}

void ConnectHandler::processRequest(
    nx::network::http::HttpServerConnection* const connection,
    nx::utils::stree::ResourceContainer authInfo,
    nx::network::http::Request request,
    nx::network::http::Response* const response,
    nx::network::http::RequestProcessedHandler completionHandler)
{
    static_cast<void>(authInfo);
    static_cast<void>(response);

    // NOTE: this validation should be done somewhere in more general place
    if (!request.requestLine.url.isValid())
        return completionHandler(network::http::StatusCode::badRequest);
    network::SocketAddress targetAddress(request.requestLine.url.host(),
        request.requestLine.url.port());
    if (!network::SocketGlobals::addressResolver()
            .isCloudHostName(targetAddress.address.toString()))
    {
        // No cloud address means direct IP.
        if (!m_settings.cloudConnect().allowIpTarget)
            return completionHandler(nx::network::http::StatusCode::forbidden);
        if (targetAddress.port == 0)
            targetAddress.port = m_settings.http().proxyTargetPort;
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

void ConnectHandler::connect(const network::SocketAddress& address,
    network::http::RequestProcessedHandler completionHandler)
{
    NX_DEBUG(this, "Connecting to '%1', socket[%2] -> socket[%3].",
        address, m_connection->socket().get(), m_targetSocket.get());

    m_targetSocket->connectAsync(
        address,
        [this, completionHandler = std::move(completionHandler)](SystemError::ErrorCode error)
        {
            NX_VERBOSE(this, "Connect result: %1.", SystemError::toString(error));

            if (error != SystemError::noError)
            {
                NX_WARNING(this, "Socket %1 connecting error: %2.",
                    m_targetSocket.get(), SystemError::toString(error));
                return completionHandler(nx::network::http::StatusCode::serviceUnavailable);
            }

            // TODO: check when is called onResponseSendHandler, is it possible to skip some data
            //      between sending response and start of tunnel
            network::http::RequestResult result(network::http::StatusCode::ok);
            result.connectionEvents.onResponseHasBeenSent =
                [targetSocket = std::move(m_targetSocket),
                    tunnelCreatedHandler = std::move(m_tunnelCreatedHandler),
                    tunnelClosedHandler = std::move(m_tunnelClosedHandler)](
                    network::http::HttpServerConnection *connection) mutable
                {
                    auto clientSocket = connection->takeSocket();
                    clientSocket->cancelIOSync(network::aio::etNone);

                    tunnelCreatedHandler(std::make_unique<Tunnel>(std::move(clientSocket),
                        std::move(targetSocket), std::move(tunnelClosedHandler)));
                };
            return completionHandler(std::move(result));
        });
}

} // namespace gateway
} // namespace cloud
} // namespace nx
