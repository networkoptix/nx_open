#include "reverse_acceptor.h"

#include <nx/network/http/empty_message_body_source.h>
#include <nx/network/socket_delegate.h>

#include "reverse_headers.h"

namespace nx {
namespace network {
namespace cloud {
namespace tcp {

namespace {

class AcceptedReverseConnection:
    public StreamSocketDelegate
{
    using base_type = StreamSocketDelegate;

public:
    AcceptedReverseConnection(
        QString foreignHostName,
        std::unique_ptr<AbstractStreamSocket> connection)
        :
        base_type(connection.get()),
        m_foreignHostName(foreignHostName),
        m_connection(std::move(connection))
    {
    }

    virtual QString getForeignHostName() const override
    {
        return m_foreignHostName;
    }

private:
    QString m_foreignHostName;
    std::unique_ptr<AbstractStreamSocket> m_connection;
};

} // namespace

ReverseAcceptor::ReverseAcceptor(ConnectHandler connectHandler):
    m_connectHandler(std::move(connectHandler)),
    m_httpServer(
        new nx::network::http::HttpStreamSocketServer(
            nullptr, &m_httpMessageDispatcher, false, // TODO: think about SSL?
            NatTraversalSupport::disabled))
{
    bindToAioThread(getAioThread());
}

ReverseAcceptor::~ReverseAcceptor()
{
    if (isInSelfAioThread())
        stopWhileInAioThread();
}

void ReverseAcceptor::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    Parent::bindToAioThread(aioThread);
    m_httpServer->bindToAioThread(aioThread);
}

bool ReverseAcceptor::start(
    String selfHostName, const SocketAddress& address, aio::AbstractAioThread* aioThread)
{
    m_selfHostName = std::move(selfHostName);
    if (aioThread)
        bindToAioThread(aioThread);

    auto registration = m_httpMessageDispatcher.registerRequestProcessor<NxRcHandler>(
        nx::network::http::kAnyPath,
        [this]() -> std::unique_ptr<NxRcHandler>
        {
            return std::make_unique<NxRcHandler>(this);
        },
        nx::network::http::StringType("OPTIONS"));

    if (m_httpConnectionInactivityTimeout)
        m_httpServer->setConnectionInactivityTimeout(*m_httpConnectionInactivityTimeout);

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

void ReverseAcceptor::setPoolSize(std::optional<size_t> value)
{
    QnMutexLocker lk(&m_dataMutex);
    m_poolSize = value;
}

void ReverseAcceptor::setHttpConnectionInactivityTimeout(
    std::chrono::milliseconds inactivityTimeout)
{
    QnMutexLocker lk(&m_dataMutex);
    m_httpConnectionInactivityTimeout = inactivityTimeout;
}

void ReverseAcceptor::setKeepAliveOptions(std::optional<KeepAliveOptions> value)
{
    QnMutexLocker lk(&m_dataMutex);
    m_keepAliveOptions = value;
}

void ReverseAcceptor::stopWhileInAioThread()
{
    m_connectHandler = nullptr;
    m_httpServer->pleaseStopSync();
}

void ReverseAcceptor::fillNxRcHeaders(nx::network::http::HttpHeaders* headers) const
{
    headers->emplace(kNxRcHostName, m_selfHostName);
    QnMutexLocker lk(&m_dataMutex);

    if (m_poolSize)
        headers->emplace(kNxRcPoolSize, QString::number(*m_poolSize).toUtf8());

    if (m_keepAliveOptions)
        headers->emplace(kNxRcKeepAliveOptions, m_keepAliveOptions->toString().toUtf8());
}

void ReverseAcceptor::saveConnection(String hostName, nx::network::http::HttpServerConnection* connection)
{
    auto socket = std::make_unique<AcceptedReverseConnection>(
        hostName,
        connection->takeSocket());
    connection->closeConnection(SystemError::badDescriptor);

    {
        QnMutexLocker lk(&m_dataMutex);
        if (m_keepAliveOptions && !socket->setKeepAlive(m_keepAliveOptions))
        {
            NX_WARNING(this, lm("Could not set keepAliveOptions=%1 to new socket(%2)")
                .arg(m_keepAliveOptions).arg(socket));

            return;
        }
    }

    post(
        [this, hostName = std::move(hostName), socket = std::move(socket)]() mutable
        {
            if (m_connectHandler)
                m_connectHandler(hostName, std::move(socket));
        });
}

//-------------------------------------------------------------------------------------------------
// ReverseAcceptor::NxRcHandler

ReverseAcceptor::NxRcHandler::NxRcHandler(ReverseAcceptor* acceptor):
    m_acceptor(acceptor)
{
}

void ReverseAcceptor::NxRcHandler::processRequest(
    nx::network::http::HttpServerConnection* const connection,
    nx::utils::stree::ResourceContainer,
    nx::network::http::Request request,
    nx::network::http::Response* const response,
    nx::network::http::RequestProcessedHandler handler)
{
    auto connectionIt = request.headers.find(kConnection);
    if (connectionIt == request.headers.end() || connectionIt->second != kUpgrade)
        return handler(nx::network::http::StatusCode::notAcceptable);

    auto upgradeIt = request.headers.find(kUpgrade);
    if (upgradeIt == request.headers.end() || !upgradeIt->second.startsWith(kNxRc))
        return handler(nx::network::http::StatusCode::notImplemented);

    String upgrade = upgradeIt->second + ", ";
    request.requestLine.version.serialize(&upgrade);
    response->headers.emplace(kConnection, kUpgrade);
    response->headers.emplace(kUpgrade, upgrade);

    auto hostNameIt = request.headers.find(kNxRcHostName);
    if (hostNameIt == request.headers.end() || hostNameIt->second.isEmpty())
        return handler(nx::network::http::StatusCode::badRequest);

    NX_VERBOSE(this, lm("Request from: %1").arg(hostNameIt->second));
    connection->setSendCompletionHandler(
        [connection, acceptor = m_acceptor, hostName = std::move(hostNameIt->second)](
            SystemError::ErrorCode code) mutable
        {
            if (code == SystemError::noError)
                acceptor->saveConnection(hostName, connection);
        });

    m_acceptor->fillNxRcHeaders(&response->headers);
    handler(nx::network::http::StatusCode::switchingProtocols);
}

} // namespace tcp
} // namespace cloud
} // namespace network
} // namespace nx
