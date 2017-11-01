#include "cloud_media_server_endpoint_verificator.h"

#include <network/module_information.h>
#include <rest/server/json_rest_result.h>

CloudMediaServerEndpointVerificator::CloudMediaServerEndpointVerificator(
    const nx::String& connectSessionId)
    :
    m_connectSessionId(connectSessionId)
{
    m_httpClient = nx_http::AsyncHttpClient::create();

    bindToAioThread(getAioThread());
}

void CloudMediaServerEndpointVerificator::bindToAioThread(
    nx::network::aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);
    m_httpClient->bindToAioThread(aioThread);
}

void CloudMediaServerEndpointVerificator::setTimeout(
    std::chrono::milliseconds timeout)
{
    m_timeout = timeout;
}

void CloudMediaServerEndpointVerificator::verifyHost(
    const SocketAddress& endpointToVerify,
    const nx::network::cloud::AddressEntry& targetHostAddress,
    nx::utils::MoveOnlyFunc<void(VerificationResult)> completionHandler)
{
    m_endpointToVerify = endpointToVerify;
    m_targetHostAddress = targetHostAddress;
    m_completionHandler = std::move(completionHandler);

    m_httpClient->bindToAioThread(getAioThread());
    if (m_timeout)
    {
        m_httpClient->setSendTimeoutMs(m_timeout->count());
        m_httpClient->setResponseReadTimeoutMs(m_timeout->count());
        m_httpClient->setMessageBodyReadTimeoutMs(m_timeout->count());
    }

    m_httpClient->doGet(
        nx::utils::Url(lit("http://%1/api/moduleInformation").arg(m_endpointToVerify.toString())),
        std::bind(&CloudMediaServerEndpointVerificator::onHttpRequestDone, this));
}

SystemError::ErrorCode CloudMediaServerEndpointVerificator::lastSystemErrorCode() const
{
    return m_lastSystemErrorCode;
}

std::unique_ptr<AbstractStreamSocket> CloudMediaServerEndpointVerificator::takeSocket()
{
    return m_httpClient->takeSocket();
}

void CloudMediaServerEndpointVerificator::stopWhileInAioThread()
{
    m_httpClient.reset();
}

void CloudMediaServerEndpointVerificator::onHttpRequestDone()
{
    NX_LOGX(lm("cross-nat %1. Finished probing %2")
        .arg(m_connectSessionId).arg(m_httpClient->url()), cl_logDEBUG2);

    if (m_httpClient->failed() &&
        (m_httpClient->lastSysErrorCode() != SystemError::noError ||
            m_httpClient->response() == nullptr))
    {
        NX_LOGX(lm("cross-nat %1. Http connect to %2 has failed: %3")
            .arg(m_connectSessionId).arg(m_endpointToVerify.toString())
            .arg(SystemError::toString(m_httpClient->lastSysErrorCode())),
            cl_logDEBUG2);
        m_lastSystemErrorCode = m_httpClient->lastSysErrorCode();
        return nx::utils::swapAndCall(
            m_completionHandler,
            VerificationResult::ioError);
    }

    if (!nx_http::StatusCode::isSuccessCode(
            m_httpClient->response()->statusLine.statusCode))
    {
        NX_LOGX(lm("cross-nat %1. Http request to %2 has failed: %3")
            .arg(m_connectSessionId).arg(m_endpointToVerify.toString())
            .arg(nx_http::StatusCode::toString(
                m_httpClient->response()->statusLine.statusCode)),
            cl_logDEBUG2);
        return nx::utils::swapAndCall(
            m_completionHandler,
            VerificationResult::notPassed);
    }

    if (!verifyHostResponse(m_httpClient))
    {
        return nx::utils::swapAndCall(
            m_completionHandler,
            VerificationResult::notPassed);
    }

    return nx::utils::swapAndCall(
        m_completionHandler,
        VerificationResult::passed);
}

bool CloudMediaServerEndpointVerificator::verifyHostResponse(
    nx_http::AsyncHttpClientPtr httpClient)
{
    const auto contentType = nx_http::getHeaderValue(
        httpClient->response()->headers, "Content-Type");
    if (Qn::serializationFormatFromHttpContentType(contentType) != Qn::JsonFormat)
    {
        NX_LOGX(lm("cross-nat %1. Received unexpected Content-Type %2 from %3")
            .args(m_connectSessionId, contentType, httpClient->url()), cl_logDEBUG2);
        return false;
    }

    QnJsonRestResult restResult;
    if (!QJson::deserialize(httpClient->fetchMessageBodyBuffer(), &restResult)
        || restResult.error != QnRestResult::Error::NoError)
    {
        NX_LOGX(lm("cross-nat %1. Error response '%2' from %3")
            .args(m_connectSessionId, restResult.errorString, httpClient->url()), cl_logDEBUG2);
        return false;
    }

    QnModuleInformation moduleInformation;
    if (!QJson::deserialize<QnModuleInformation>(restResult.reply, &moduleInformation)
        || !moduleInformation.cloudId().endsWith(m_targetHostAddress.host.toString()))
    {
        NX_LOGX(lm("cross-nat %1. Connected to a wrong server (%2) instead of %3")
            .args(m_connectSessionId, moduleInformation.cloudId(), m_targetHostAddress.host),
            cl_logDEBUG2);
        return false;
    }

    return true;
}
