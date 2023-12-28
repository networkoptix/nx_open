// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cdb_connection.h"

#include <nx/network/socket_common.h>

#include "cdb_request_path.h"
#include "data/module_info.h"

namespace nx::cloud::db::client {

Connection::Connection(
    const nx::utils::Url& baseUrl,
    nx::network::ssl::AdapterFunc adapterFunc)
    :
    m_requestExecutor(baseUrl, std::move(adapterFunc)),
    m_accountManager(&m_requestExecutor),
    m_systemManager(&m_requestExecutor),
    m_organizationManager(&m_requestExecutor),
    m_authProvider(&m_requestExecutor),
    m_maintenanceManager(&m_requestExecutor),
    m_oauthManager(&m_requestExecutor),
    m_twoFactorAuthManager(&m_requestExecutor),
    m_batchUserProcessingManager(&m_requestExecutor)
{
    m_requestExecutor.setCacheEnabled(true);

    bindToAioThread(m_requestExecutor.getAioThread());
}

api::AccountManager* Connection::accountManager()
{
    return &m_accountManager;
}

api::SystemManager* Connection::systemManager()
{
    return &m_systemManager;
}

api::OrganizationManager* Connection::organizationManager()
{
    return &m_organizationManager;
}

api::AuthProvider* Connection::authProvider()
{
    return &m_authProvider;
}

api::MaintenanceManager* Connection::maintenanceManager()
{
    return &m_maintenanceManager;
}

api::OauthManager* Connection::oauthManager()
{
    return &m_oauthManager;
}

api::TwoFactorAuthManager* Connection::twoFactorAuthManager()
{
    return &m_twoFactorAuthManager;
}

api::BatchUserProcessingManager* Connection::batchUserProcessingManager()
{
    return &m_batchUserProcessingManager;
}

void Connection::bindToAioThread(
    nx::network::aio::AbstractAioThread* aioThread)
{
    m_requestExecutor.bindToAioThread(aioThread);
}

void Connection::setCredentials(nx::network::http::Credentials credentials)
{
    m_requestExecutor.httpClientOptions().setCredentials(credentials);
}

void Connection::setProxyVia(
    const std::string& proxyHost,
    std::uint16_t proxyPort,
    nx::network::http::Credentials credentials,
    nx::network::ssl::AdapterFunc adapterFunc)
{
    m_requestExecutor.httpClientOptions().setProxyCredentials(credentials);
    m_requestExecutor.httpClientOptions().setProxyVia(
        {proxyHost, proxyPort}, /*isSecure*/ true, adapterFunc);
}

void Connection::setRequestTimeout(std::chrono::milliseconds timeout)
{
    m_requestExecutor.setRequestTimeout(timeout);
    m_requestExecutor.httpClientOptions().setTimeouts({
        .sendTimeout = timeout,
        .responseReadTimeout = timeout,
        .messageBodyReadTimeout = timeout});
}

std::chrono::milliseconds Connection::requestTimeout() const
{
    return m_requestExecutor.httpClientOptions().timeouts().responseReadTimeout;
}

void Connection::setAdditionalHeaders(nx::network::http::HttpHeaders headers)
{
    m_requestExecutor.httpClientOptions().setAdditionalHeaders(std::move(headers));
}

void Connection::ping(
    std::function<void(api::ResultCode, api::ModuleInfo)> completionHandler)
{
    m_requestExecutor.makeAsyncCall<api::ModuleInfo>(
        nx::network::http::Method::get,
        kPingPath,
        {}, //query
        std::move(completionHandler));
}

void Connection::setCacheEnabled(bool enabled)
{
    m_requestExecutor.setCacheEnabled(enabled);
}

} // namespace nx::cloud::db::client
