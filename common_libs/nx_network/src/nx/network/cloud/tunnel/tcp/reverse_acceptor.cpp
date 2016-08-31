#include "reverse_acceptor.h"

#include "reverse_headers.h"

namespace nx {
namespace network {
namespace cloud {
namespace tcp {

namespace {

struct UpgradeMsgBody: nx_http::AbstractMsgBodySource
{
    void pleaseStop(nx::utils::MoveOnlyFunc<void()>) override {}

    aio::AbstractAioThread* getAioThread() const override { return nullptr; }
    void bindToAioThread(aio::AbstractAioThread*) override {}
    void post(nx::utils::MoveOnlyFunc<void()>) override {}
    void dispatch(nx::utils::MoveOnlyFunc<void()>) override {}

    String mimeType() const override { return String(); }
    boost::optional<uint64_t> contentLength() const override { return boost::none; }
    void readAsync(utils::MoveOnlyFunc<void(SystemError::ErrorCode, Buffer)>) override {}
};

} // namespace

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

void ReverseAcceptor::saveConnection(String hostName, nx_http::HttpServerConnection* connection) const
{
    auto socket = connection->takeSocket();
    connection->closeConnection(SystemError::badDescriptor);

    auto rawSocket = dynamic_cast<AbstractStreamSocket*>(socket.get());
    NX_CRITICAL(rawSocket);

    {
        QnMutexLocker lk(&m_dataMutex);
        if (m_keepAliveOptions && !rawSocket->setKeepAlive(m_keepAliveOptions))
        {
            NX_LOGX(lm("Could not set keepAliveOptions=%1 to new socket(%2)")
                .arg(m_keepAliveOptions).arg(socket), cl_logWARNING);

            return;
        }
    }

    socket.release();
    m_httpServer->post(
        [this, hostName=std::move(hostName),
            socket = std::unique_ptr<AbstractStreamSocket>(rawSocket)]() mutable
        {
            m_connectHandler(hostName, std::move(socket));
        });
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
    std::function<void(
        const nx_http::StatusCode::Value code,
        std::unique_ptr<nx_http::AbstractMsgBodySource> dataSource )> handler)
{
    auto connectionIt = request.headers.find(kConnection);
    if (connectionIt == request.headers.end() || connectionIt->second != kUpgrade)
        return handler(nx_http::StatusCode::notAcceptable, nullptr);

    auto upgradeIt = request.headers.find(kUpgrade);
    if (upgradeIt == request.headers.end() || !upgradeIt->second.startsWith(kNxRc))
        return handler(nx_http::StatusCode::notImplemented, nullptr);

    String upgrade = upgradeIt->second + ", ";
    request.requestLine.version.serialize(&upgrade);
    response->headers.emplace(kConnection, kUpgrade);
    response->headers.emplace(kUpgrade, upgrade);

    auto hostNameIt = request.headers.find(kNxRcHostName);
    if (hostNameIt == request.headers.end() || hostNameIt->second.isEmpty())
        return handler(nx_http::StatusCode::badRequest, nullptr);

    NX_LOGX(lm("Request from: %1").arg(hostNameIt->second), cl_logDEBUG2);
    connection->setSendCompletionHandler(
        [connection, acceptor = m_acceptor, hostName = std::move(hostNameIt->second)](
            SystemError::ErrorCode code)
        {
            if (code == SystemError::noError)
                acceptor->saveConnection(hostName, connection);
        });

    m_acceptor->fillNxRcHeaders(&response->headers);
    handler(nx_http::StatusCode::upgrade, std::make_unique<UpgradeMsgBody>());
}

} // namespace tcp
} // namespace cloud
} // namespace network
} // namespace nx
