/**********************************************************
* Jul 7, 2016
* akolesnikov
***********************************************************/

#include "direct_endpoint_connector.h"

#include <nx/fusion/serialization/json.h>
#include <nx/network/http/asynchttpclient.h>
#include <nx/utils/log/log.h>

#include <network/module_information.h>

#include "direct_endpoint_tunnel.h"

namespace nx {
namespace network {
namespace cloud {
namespace tcp {

DirectEndpointConnector::DirectEndpointConnector(
    AddressEntry targetHostAddress,
    nx::String connectSessionId)
:
    m_targetHostAddress(std::move(targetHostAddress)),
    m_connectSessionId(std::move(connectSessionId))
{
}

DirectEndpointConnector::~DirectEndpointConnector()
{
    stopWhileInAioThread();
}

void DirectEndpointConnector::stopWhileInAioThread()
{
    m_connections.clear();
}

int DirectEndpointConnector::getPriority() const
{
    //TODO #ak
    return 0;
}

void DirectEndpointConnector::connect(
    const hpm::api::ConnectResponse& response,
    std::chrono::milliseconds timeout,
    ConnectCompletionHandler handler)
{
    using namespace std::placeholders;

    SystemError::ErrorCode sysErrorCode = SystemError::noError;

    // Performing /api/moduleInformation HTTP requests since 
    //  currently only mediaserver can be on remote side

    NX_ASSERT(!response.forwardedTcpEndpointList.empty());
    for (const SocketAddress& endpoint: response.forwardedTcpEndpointList)
    {
        auto httpClient = nx_http::AsyncHttpClient::create();
        httpClient->bindToAioThread(getAioThread());
        httpClient->setSendTimeoutMs(timeout.count());
        httpClient->setResponseReadTimeoutMs(timeout.count());
        httpClient->setMessageBodyReadTimeoutMs(timeout.count());
        m_connections.push_back(ConnectionContext{endpoint, std::move(httpClient)});
    }

    post(
        [this, sysErrorCode, handler = std::move(handler)]() mutable
        {
            if (m_connections.empty())
            {
                //reporting error
                handler(
                    nx::hpm::api::NatTraversalResultCode::tcpConnectFailed,
                    sysErrorCode,
                    nullptr);
                return;
            }

            m_completionHandler = std::move(handler);
            for (auto it = m_connections.begin(); it != m_connections.end(); ++it)
            {
                it->httpClient->doGet(
                    QUrl(lit("http://%1/api/moduleInformation").arg(it->endpoint.toString())),
                    std::bind(&DirectEndpointConnector::onHttpRequestDone, this, _1, it));
            }
    });
}

const AddressEntry& DirectEndpointConnector::targetPeerAddress() const
{
    return m_targetHostAddress;
}

void DirectEndpointConnector::onHttpRequestDone(
    nx_http::AsyncHttpClientPtr httpClient,
    std::list<ConnectionContext>::iterator socketIter)
{
    auto connectionContext = std::move(*socketIter);
    m_connections.erase(socketIter);

    if (httpClient->failed() &&
        (httpClient->lastSysErrorCode() != SystemError::noError ||
         httpClient->response() == nullptr))
    {
        NX_LOGX(lm("cross-nat %1. Http connect to %2 has failed: %3")
            .arg(m_connectSessionId).arg(connectionContext.endpoint.toString())
            .arg(SystemError::toString(httpClient->lastSysErrorCode())),
            cl_logDEBUG2);
        return reportErrorOnEndpointVerificationFailure(
            nx::hpm::api::NatTraversalResultCode::tcpConnectFailed,
            httpClient->lastSysErrorCode());
    }

    if (!nx_http::StatusCode::isSuccessCode(
            httpClient->response()->statusLine.statusCode))
    {
        NX_LOGX(lm("cross-nat %1. Http request to %2 has failed: %3")
            .arg(m_connectSessionId).arg(connectionContext.endpoint.toString())
            .arg(nx_http::StatusCode::toString(
                httpClient->response()->statusLine.statusCode)),
            cl_logDEBUG2);
        return reportErrorOnEndpointVerificationFailure(
            nx::hpm::api::NatTraversalResultCode::endpointVerificationFailure,
            SystemError::noError);
    }

    if (!verifyHostResponse(httpClient))
    {
        return reportErrorOnEndpointVerificationFailure(
            nx::hpm::api::NatTraversalResultCode::endpointVerificationFailure,
            SystemError::noError);
    }

    reportSuccessfulVerificationResult(
        std::move(connectionContext.endpoint),
        httpClient->takeSocket());
}

void DirectEndpointConnector::reportErrorOnEndpointVerificationFailure(
    nx::hpm::api::NatTraversalResultCode resultCode,
    SystemError::ErrorCode sysErrorCode)
{
    if (!m_connections.empty())
        return; //waiting for completion of other connections
    auto handler = std::move(m_completionHandler);
    m_completionHandler = nullptr;
    return handler(
        resultCode,
        sysErrorCode,
        nullptr);
}

bool DirectEndpointConnector::verifyHostResponse(
    nx_http::AsyncHttpClientPtr httpClient)
{
    const auto contentType = nx_http::getHeaderValue(
        httpClient->response()->headers, "Content-Type");
    if (Qn::serializationFormatFromHttpContentType(contentType) != Qn::JsonFormat)
    {
        NX_LOGX(lm("cross-nat %1. Received unexpected Content-Type %2 from %3")
            .arg(m_connectSessionId).arg(contentType).str(httpClient->url()),
            cl_logDEBUG2);
        return false;
    }

    bool parseResult = false;
    auto moduleInformation = 
        QJson::deserialized<QnModuleInformation>(
            httpClient->fetchMessageBodyBuffer(), QnModuleInformation(), &parseResult);
    if (!parseResult)
    {
        NX_LOGX(lm("cross-nat %1. Failed to parse response from %2")
            .arg(m_connectSessionId).str(httpClient->url()),
            cl_logDEBUG2);
        return false;
    }

    if (m_targetHostAddress.host.toString().indexOf(moduleInformation.cloudSystemId) == -1)
    {
        NX_LOGX(lm("cross-nat %1. Received unexpected cloud system id %2 from %3")
            .arg(m_connectSessionId).arg(moduleInformation.cloudSystemId)
            .str(httpClient->url()),
            cl_logDEBUG2);
        return false;
    }

    return true;
}

void DirectEndpointConnector::reportSuccessfulVerificationResult(
    SocketAddress endpoint,
    std::unique_ptr<AbstractStreamSocket> streamSocket)
{
    m_connections.clear();

    auto tunnel =
        std::make_unique<DirectTcpEndpointTunnel>(
            getAioThread(),
            m_connectSessionId,
            std::move(endpoint),
            std::move(streamSocket));

    auto handler = std::move(m_completionHandler);
    m_completionHandler = nullptr;
    handler(
        nx::hpm::api::NatTraversalResultCode::ok,
        SystemError::noError,
        std::move(tunnel));
}

} // namespace tcp
} // namespace cloud
} // namespace network
} // namespace nx
