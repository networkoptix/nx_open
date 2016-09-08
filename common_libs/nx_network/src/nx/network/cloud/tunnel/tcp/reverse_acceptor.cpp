#include "reverse_acceptor.h"

#include <nx/network/http/empty_message_body_source.h>

#include "reverse_headers.h"

namespace nx {
namespace network {
namespace cloud {
namespace tcp {

ReverseAcceptor::ReverseAcceptor(String selfHostName, ConnectHandler connectHandler):
    m_selfHostName(std::move(selfHostName)),
    m_connectHandler(std::move(connectHandler)),
    m_httpServer(new nx_http::HttpStreamSocketServer(
        nullptr, &m_httpMessageDispatcher, false, // TODO: think about SSL?
        SocketFactory::NatTraversalType::nttDisabled))
{
}

bool ReverseAcceptor::start(const SocketAddress& address, aio::AbstractAioThread* aioThread)
{
    if (aioThread)
        m_httpServer->bindToAioThread(aioThread);

    auto registration = m_httpMessageDispatcher.registerRequestProcessor<NxRcHandler>(
        nx_http::kAnyPath,
        [this]() -> std::unique_ptr<NxRcHandler>
        {
            return std::make_unique<NxRcHandler>(this);
        },
        nx_http::StringType("OPTIONS"));

    NX_CRITICAL(registration);
    return m_httpServer->bind(address) && m_httpServer->listen();
}

SocketAddress ReverseAcceptor::address() const
{
    return m_httpServer->address();
}

String ReverseAcceptor::selfHostName() const
{
    return m_selfHostName;
}

void ReverseAcceptor::setPoolSize(boost::optional<size_t> value)
{
    QnMutexLocker lk(&m_dataMutex);
    m_poolSize = value;
}

void ReverseAcceptor::setKeepAliveOptions(boost::optional<KeepAliveOptions> value)
{
    QnMutexLocker lk(&m_dataMutex);
    m_keepAliveOptions = value;
}

void ReverseAcceptor::fillNxRcHeaders(nx_http::HttpHeaders* headers) const
{
    headers->emplace(kNxRcHostName, m_selfHostName);
    QnMutexLocker lk(&m_dataMutex);

    if (m_poolSize)
        headers->emplace(kNxRcPoolSize, QString::number(*m_poolSize).toUtf8());

    if (m_keepAliveOptions)
        headers->emplace(kNxRcKeepAliveOptions, m_keepAliveOptions->toString().toUtf8());
}

void ReverseAcceptor::newClient(String hostName, nx_http::HttpServerConnection* connection) const
{
    auto socket = connection->takeSocket();
    connection->closeConnection(SystemError::badDescriptor);

    auto rawSocket = dynamic_cast<AbstractStreamSocket*>(socket.get());
    NX_CRITICAL(rawSocket);

    socket.release();
    m_connectHandler(hostName, std::unique_ptr<AbstractStreamSocket>(rawSocket));
}

ReverseAcceptor::NxRcHandler::NxRcHandler(const ReverseAcceptor* acceptor):
    m_acceptor(acceptor)
{
}

void ReverseAcceptor::NxRcHandler::processRequest(
    nx_http::HttpServerConnection* const connection,
    stree::ResourceContainer,
    nx_http::Request request,
    nx_http::Response* const response,
    nx_http::HttpRequestProcessedHandler handler)
{
    auto connectionIt = request.headers.find(kConnection);
    if (connectionIt == request.headers.end() || connectionIt->second != kUpgrade)
        return handler(
            nx_http::StatusCode::notAcceptable,
            nullptr,
            nx_http::ConnectionEvents());

    auto upgradeIt = request.headers.find(kUpgrade);
    if (upgradeIt == request.headers.end() || !upgradeIt->second.startsWith(kNxRc))
        return handler(
            nx_http::StatusCode::notImplemented,
            nullptr,
            nx_http::ConnectionEvents());

    String upgrade = upgradeIt->second + ", ";
    request.requestLine.version.serialize(&upgrade);
    response->headers.emplace(kConnection, kUpgrade);
    response->headers.emplace(kUpgrade, upgrade);

    auto hostNameIt = request.headers.find(kNxRcHostName);
    if (hostNameIt == request.headers.end() || hostNameIt->second.isEmpty())
        return handler(
            nx_http::StatusCode::badRequest,
            nullptr,
            nx_http::ConnectionEvents());

    NX_LOGX(lm("request from: %1").arg(hostNameIt->second), cl_logDEBUG2);
    connection->setSendCompletionHandler(
        [connection, acceptor = m_acceptor, hostName = std::move(hostNameIt->second)](
            SystemError::ErrorCode code)
        {
            if (code == SystemError::noError)
                acceptor->newClient(hostName, connection);
        });

    m_acceptor->fillNxRcHeaders(&response->headers);
    handler(
        nx_http::StatusCode::upgrade,
        std::make_unique<nx_http::EmptyMessageBodySource>(nx::String(), boost::none),
        nx_http::ConnectionEvents());
}

} // namespace tcp
} // namespace cloud
} // namespace network
} // namespace nx
