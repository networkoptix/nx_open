#include "reverse_connector.h"

#include <nx/utils/std/cpp14.h>
#include <nx/utils/log/log.h>

namespace nx {
namespace network {
namespace cloud {
namespace tcp {

ReverseConnector::ReverseConnector(String selfName, String targetName):
    m_selfName(std::move(selfName)),
    m_targetName(std::move(targetName))
{
}

void ReverseConnector::connect(const SocketAddress& endpoint, ConnectHandler handler)
{
    // TODO: use timeout
    m_httpClient = nx_http::AsyncHttpClient::create();
    m_httpClient->addAdditionalHeader(kUpgrade, kNxRc);
    m_httpClient->setConnectionHeader(kUpgrade);
    m_httpClient->addAdditionalHeader(kNxRcHostName, m_selfName);

    QObject::connect(
        m_httpClient.get(), &nx_http::AsyncHttpClient::done,
        [this, handler=std::move(handler)](nx_http::AsyncHttpClientPtr)
        {
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
        });

    // TODO: think about HTTPS
    m_httpClient->doOptions(endpoint.toUrl());
}

std::unique_ptr<ExtendedStreamSocket> ReverseConnector::takeSocket()
{
    auto socket = std::make_unique<ExtendedStreamSocket>(m_httpClient->takeSocket());
    socket->injectRecvData(m_httpClient->fetchMessageBodyBuffer());
    return socket;
}

size_t ReverseConnector::getPoolSize() const
{
    const auto value = m_httpClient->response()->headers.find(kNxRcPoolSize);
    if (value == m_httpClient->response()->headers.end())
        return 0;

    return (size_t)QString::fromUtf8(value->second).toInt();
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

    const auto connection = headers.find("Connection");
    if (connection == headers.end() || connection->second != kUpgrade)
        return SystemError::invalidData;

    const auto hostName = headers.find(kNxRcHostName);
    if (hostName == headers.end() || hostName->second != m_targetName)
        return SystemError::hostNotFound;

    return SystemError::noError;
}

} // namespace tcp
} // namespace cloud
} // namespace network
} // namespace nx
