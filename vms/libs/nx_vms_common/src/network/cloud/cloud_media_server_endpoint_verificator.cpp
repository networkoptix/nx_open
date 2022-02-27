// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_media_server_endpoint_verificator.h"

#include <nx/network/url/url_builder.h>
#include <nx/network/rest/result.h>

#include <nx/utils/log/log.h>
#include <nx/vms/api/data/module_information.h>

CloudMediaServerEndpointVerificator::CloudMediaServerEndpointVerificator(
    const std::string& connectSessionId)
    :
    m_connectSessionId(connectSessionId)
{
    m_httpClient = nx::network::http::AsyncHttpClient::create(nx::network::ssl::kDefaultCertificateCheck);

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
    const nx::network::SocketAddress& endpointToVerify,
    const nx::network::AddressEntry& targetHostAddress,
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
        nx::network::url::Builder()
            .setScheme(nx::network::http::kUrlSchemeName)
            .setEndpoint(m_endpointToVerify)
            .setPath("/api/moduleInformation").toUrl(),
        std::bind(&CloudMediaServerEndpointVerificator::onHttpRequestDone, this));
}

SystemError::ErrorCode CloudMediaServerEndpointVerificator::lastSystemErrorCode() const
{
    return m_lastSystemErrorCode;
}

std::unique_ptr<nx::network::AbstractStreamSocket> CloudMediaServerEndpointVerificator::takeSocket()
{
    return m_httpClient->takeSocket();
}

void CloudMediaServerEndpointVerificator::stopWhileInAioThread()
{
    m_httpClient.reset();
}

void CloudMediaServerEndpointVerificator::onHttpRequestDone()
{
    if (m_httpClient->failed() &&
        (m_httpClient->lastSysErrorCode() != SystemError::noError ||
            m_httpClient->response() == nullptr))
    {
        NX_VERBOSE(this, "cross-nat %1. Failed to verify %2. Http connect has failed: %3",
            m_connectSessionId, m_endpointToVerify.toString(),
                SystemError::toString(m_httpClient->lastSysErrorCode()));
        m_lastSystemErrorCode = m_httpClient->lastSysErrorCode();
        return nx::utils::swapAndCall(
            m_completionHandler,
            VerificationResult::ioError);
    }

    if (!nx::network::http::StatusCode::isSuccessCode(
            m_httpClient->response()->statusLine.statusCode))
    {
        NX_VERBOSE(this, "cross-nat %1. Failed to verify %2. Http connect has failed: %3",
            m_connectSessionId, m_endpointToVerify.toString(),
            nx::network::http::StatusCode::toString(
                m_httpClient->response()->statusLine.statusCode));
        return nx::utils::swapAndCall(
            m_completionHandler,
            VerificationResult::notPassed);
    }

    if (!verifyHostResponse(m_httpClient))
    {
        NX_VERBOSE(this, "cross-nat %1. Failed to verify %2",
            m_connectSessionId, m_httpClient->url());

        return nx::utils::swapAndCall(
            m_completionHandler,
            VerificationResult::notPassed);
    }

    NX_VERBOSE(this, "cross-nat %1. URL %2 has been verified. Target server confirmed",
        m_connectSessionId, m_httpClient->url());

    return nx::utils::swapAndCall(
        m_completionHandler,
        VerificationResult::passed);
}

bool CloudMediaServerEndpointVerificator::verifyHostResponse(
    nx::network::http::AsyncHttpClientPtr httpClient)
{
    const auto contentType = nx::network::http::getHeaderValue(
        httpClient->response()->headers, "Content-Type");
    if (Qn::serializationFormatFromHttpContentType(contentType) != Qn::JsonFormat)
    {
        NX_VERBOSE(this, nx::format("cross-nat %1. Received unexpected Content-Type %2 from %3")
            .args(m_connectSessionId, contentType, httpClient->url()));
        return false;
    }

    nx::network::rest::JsonResult restResult;
    if (!QJson::deserialize(httpClient->fetchMessageBodyBuffer(), &restResult)
        || restResult.error != nx::network::rest::Result::Error::NoError)
    {
        NX_VERBOSE(this, nx::format("cross-nat %1. Error response '%2' from %3")
            .args(m_connectSessionId, restResult.errorString, httpClient->url()));
        return false;
    }

    nx::vms::api::ModuleInformation moduleInformation;
    if (!QJson::deserialize(restResult.reply, &moduleInformation)
        || !moduleInformation.cloudId().endsWith(m_targetHostAddress.host.toString().c_str()))
    {
        NX_VERBOSE(this, nx::format("cross-nat %1. Connected to a wrong server (%2) instead of %3")
            .args(m_connectSessionId, moduleInformation.cloudId(), m_targetHostAddress.host));
        return false;
    }

    return true;
}
