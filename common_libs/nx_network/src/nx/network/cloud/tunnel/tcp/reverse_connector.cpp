#include "reverse_connector.h"

#include "reverse_headers.h"

#include <nx/utils/std/cpp14.h>
#include <nx/utils/log/log.h>

namespace nx {
namespace network {
namespace cloud {
namespace tcp {

ReverseConnector::ReverseConnector(
    String selfHostName, String targetHostName, aio::AbstractAioThread* aioThread)
:
    m_targetHostName(std::move(targetHostName))
{
    m_httpClient = nx_http::AsyncHttpClient::create();
    if (aioThread)
        m_httpClient->bindToAioThread(aioThread);

    m_httpClient->addAdditionalHeader(kConnection, kUpgrade);
    m_httpClient->addAdditionalHeader(kUpgrade, kNxRc);
    m_httpClient->addAdditionalHeader(kNxRcHostName, selfHostName);
}

void ReverseConnector::setHttpTimeouts(nx_http::AsyncHttpClient::Timeouts timeouts)
{
    m_httpClient->setSendTimeoutMs(timeouts.sendTimeout.count());
    m_httpClient->setResponseReadTimeoutMs(timeouts.responseReadTimeout.count());
    m_httpClient->setMessageBodyReadTimeoutMs(timeouts.messageBodyReadTimeout.count());
}

void ReverseConnector::connect(const SocketAddress& endpoint, ConnectHandler handler)
{
    m_handler = std::move(handler);
    const auto onHttpDone =
        [this](nx_http::AsyncHttpClientPtr)
        {
            decltype(m_handler) handler;
            handler.swap(m_handler);
            if (!handler)
                return;

            m_handler = nullptr;
            if (m_httpClient->failed() || !m_httpClient->response())
                return handler(SystemError::connectionRefused);

            if (m_httpClient->response()->statusLine.statusCode != 101)
            {
                NX_LOG(lm("Upexpected status: (%1) %2")
                    .arg(m_httpClient->response()->statusLine.statusCode)
                    .arg(m_httpClient->response()->statusLine.reasonPhrase), cl_logDEBUG1);

                return handler(SystemError::connectionAbort);
            }

            handler(processHeaders(m_httpClient->response()->headers));
        };

    QObject::connect(m_httpClient.get(), &nx_http::AsyncHttpClient::responseReceived, onHttpDone);
    QObject::connect(m_httpClient.get(), &nx_http::AsyncHttpClient::done, onHttpDone);
    m_httpClient->doOptions(endpoint.toUrl());
}

std::unique_ptr<BufferedStreamSocket> ReverseConnector::takeSocket()
{
    auto socket = std::make_unique<BufferedStreamSocket>(m_httpClient->takeSocket());
    auto buffer = m_httpClient->fetchMessageBodyBuffer();

    if (buffer.size())
        socket->injectRecvData(std::move(buffer));

    if (socket->setSendTimeout(0) && socket->setRecvTimeout(0))
        return socket;

    NX_LOGX(lm("Could not disable timeouts on HTTP socket: %1")
        .arg(SystemError::getLastOSErrorText()), cl_logDEBUG1);

    return nullptr;
}

boost::optional<size_t> ReverseConnector::getPoolSize() const
{
    const auto value = m_httpClient->response()->headers.find(kNxRcPoolSize);
    if (value == m_httpClient->response()->headers.end())
        return boost::none;

    bool isOk;
    auto result = (size_t)QString::fromUtf8(value->second).toInt(&isOk);

    if (isOk)
        return result;
    else
        return boost::none;
}

boost::optional<KeepAliveOptions> ReverseConnector::getKeepAliveOptions() const
{
    const auto value = m_httpClient->response()->headers.find(kNxRcKeepAliveOptions);
    if (value == m_httpClient->response()->headers.end())
        return boost::none;

    return KeepAliveOptions::fromString(QString::fromUtf8(value->second));
}

SystemError::ErrorCode ReverseConnector::processHeaders(const nx_http::HttpHeaders& headers)
{
    const auto upgrade = headers.find(kUpgrade);
    if (upgrade == headers.end() || !upgrade->second.startsWith(kNxRc))
        return SystemError::invalidData;

    const auto connection = headers.find(kConnection);
    if (connection == headers.end() || connection->second != kUpgrade)
        return SystemError::invalidData;

    const auto hostName = headers.find(kNxRcHostName);
    if (hostName == headers.end() || hostName->second != m_targetHostName)
        return SystemError::hostNotFound;

    return SystemError::noError;
}

} // namespace tcp
} // namespace cloud
} // namespace network
} // namespace nx
