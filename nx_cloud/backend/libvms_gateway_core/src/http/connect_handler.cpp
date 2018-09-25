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

ConnectHandler::ConnectHandler(const conf::Settings& settings):
    m_settings(settings)
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
        NX_INFO(this, lm("Failed to set socket options. %1")
            .arg(SystemError::getLastOSErrorText()));

        return completionHandler(nx::network::http::StatusCode::internalServerError);
    }

    m_request = std::move(request);
    m_connection = connection;
    m_completionHandler = std::move(completionHandler);
    connect(targetAddress);
}

void ConnectHandler::closeConnection(
    SystemError::ErrorCode /*closeReason*/,
    nx::network::http::deprecated::AsyncMessagePipeline* /*connection*/)
{
    m_connectionSocket.reset();
    m_targetSocket.reset();
}

void ConnectHandler::connect(const network::SocketAddress& address)
{
    NX_DEBUG(this, lm("Connecting to '%1', socket[%2] -> socket[%3]").arg(address)
        .arg(m_connection->socket()).arg(m_targetSocket));

    // NOTE: Socket is taken before connect because otherwise some data received from client after
    // CONNECT request can be parsed by HTTP connection parser and do not get here.
    m_connectionSocket = m_connection->takeSocket();
    m_connectionSocket->cancelIOSync(network::aio::etNone);

    m_targetSocket->connectAsync(
        address,
        [this](SystemError::ErrorCode result)
        {
            NX_VERBOSE(this, lm("Connect result: %1")
                .arg(SystemError::toString(result)));

            m_request.requestLine.version.serialize(&m_connectionBuffer);
            if (result != SystemError::noError)
            {
                m_connectionBuffer.append(Buffer(" 503 Service Unavailable\r\n\r\n"));
                m_connectionSocket->sendAsync(
                    m_connectionBuffer,
                    [this, result](SystemError::ErrorCode, size_t)
                    {
                        return socketError(m_targetSocket.get(), result);
                    });
            }
            else
            {
                m_connectionBuffer.append(Buffer(" 200 Connection estabilished\r\n\r\n"));

                m_connectionSocket->sendAsync(
                    m_connectionBuffer,
                    [this](SystemError::ErrorCode result, size_t)
                    {
                        if (result != SystemError::noError)
                            return socketError(m_connectionSocket.get(), result);

                        m_connectionBuffer.reserve(kDefaultBufferSize);
                        m_connectionBuffer.resize(0);
                        stream(m_connectionSocket.get(), m_targetSocket.get(), &m_connectionBuffer);

                        m_targetBuffer.reserve(kDefaultBufferSize);
                        m_targetBuffer.resize(0);
                        stream(m_targetSocket.get(), m_connectionSocket.get(), &m_targetBuffer);
                    });
            }
        });
}

void ConnectHandler::socketError(Socket* socket, SystemError::ErrorCode error)
{
    NX_WARNING(this, "Socket %1 returned error: %2", socket, SystemError::toString(error));

    const auto handler = std::move(m_completionHandler);
    handler(nx::network::http::StatusCode::serviceUnavailable);
}

void ConnectHandler::stream(Socket* source, Socket* target, Buffer* buffer)
{
    source->readSomeAsync(
        buffer,
        [=](SystemError::ErrorCode result, size_t size)
        {
            if (result != SystemError::noError)
                return socketError(source, result);

            if (size == 0)
            {
                const auto handler = std::move(m_completionHandler);
                return handler(nx::network::http::StatusCode::ok);
            }


            target->sendAsync(
                *buffer,
                [=](SystemError::ErrorCode result, size_t)
            {
                if (result != SystemError::noError)
                    return socketError(target, result);

                stream(source, target, buffer);
            });
        });
}

} // namespace gateway
} // namespace cloud
} // namespace nx
