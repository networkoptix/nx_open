#include "connect_handler.h"

#include <nx/network/socket_global.h>
#include <nx/network/http/buffer_source.h>
#include <nx/network/cloud/address_resolver.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>

#include "settings.h"
#include "run_time_options.h"

namespace nx {
namespace cloud {
namespace gateway {

static const int kDefaultBufferSize = 16 * 1024;

ConnectHandler::ConnectHandler(
    const conf::Settings& settings,
    const conf::RunTimeOptions& runTimeOptions)
:
    m_settings(settings),
    m_runTimeOptions(runTimeOptions)
{
}

void ConnectHandler::processRequest(
    nx_http::HttpServerConnection* const connection,
    nx::utils::stree::ResourceContainer authInfo,
    nx_http::Request request,
    nx_http::Response* const response,
    nx_http::RequestProcessedHandler completionHandler)
{
    static_cast<void>(authInfo);
    static_cast<void>(response);

    SocketAddress targetAddress(request.requestLine.url.path());
    if (!network::SocketGlobals::addressResolver()
            .isCloudHostName(targetAddress.address.toString()))
    {
        // No cloud address means direct IP
        if (!m_settings.cloudConnect().allowIpTarget)
            return completionHandler(nx_http::StatusCode::forbidden);

        if (targetAddress.port == 0)
            targetAddress.port = m_settings.http().proxyTargetPort;
    }

    m_targetSocket = SocketFactory::createStreamSocket();
    m_targetSocket->bindToAioThread(connection->getAioThread());
    if (!m_targetSocket->setNonBlockingMode(true) ||
        !m_targetSocket->setRecvTimeout(m_settings.tcp().recvTimeout) ||
        !m_targetSocket->setSendTimeout(m_settings.tcp().sendTimeout))
    {
        NX_LOGX(lm("Failed to set socket options. %1")
            .arg(SystemError::getLastOSErrorText()), cl_logINFO);

        return completionHandler(nx_http::StatusCode::internalServerError);
    }

    m_request = std::move(request);
    m_connection = connection;
    m_completionHandler = std::move(completionHandler);
    connect(targetAddress);
}

void ConnectHandler::closeConnection(
    SystemError::ErrorCode /*closeReason*/,
    nx_http::deprecated::AsyncMessagePipeline* /*connection*/)
{
    m_connectionSocket.reset();
    m_targetSocket.reset();
}

void ConnectHandler::connect(const SocketAddress& address)
{
    NX_LOGX(lm("Connecting to '%1', socket[%2] -> socket[%3]").arg(address)
        .arg(m_connectionSocket).arg(m_targetSocket), cl_logDEBUG1);

    m_targetSocket->connectAsync(
        address,
        [this](SystemError::ErrorCode result)
        {
            NX_LOGX(lm("Connect result: %1")
                .arg(SystemError::toString(result)), cl_logDEBUG2);

            if (result != SystemError::noError)
                return socketError(m_targetSocket.get(), result);

            // TODO: #mux Find a way to steal socket after sending an automatic response
            m_request.requestLine.version.serialize(&m_connectionBuffer);
            m_connectionBuffer.append(Buffer(" 200 Connection estabilished\r\n\r\n"));

            // TODO: #mux Currently there is a problem as m_connection may have some data
            //  in it's buffer (client does not has to wait for CONNECT response before
            //  sending data through)
            m_connectionSocket = m_connection->takeSocket();
            m_connectionSocket->cancelIOSync(network::aio::etNone);
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
        });
}

void ConnectHandler::socketError(Socket* socket, SystemError::ErrorCode error)
{
    NX_LOGX(lm("Socket %1 returned error %2")
        .arg(socket).arg(SystemError::toString(error)), cl_logDEBUG1);

    const auto handler = std::move(m_completionHandler);
    handler(nx_http::StatusCode::serviceUnavailable);
}

void ConnectHandler::stream(Socket* source, Socket* target, Buffer* buffer)
{
    source->readSomeAsync(
        buffer,
        [=](SystemError::ErrorCode result, size_t)
        {
            if (result != SystemError::noError)
                return socketError(source, result);

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
