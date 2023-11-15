// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "async_http_requests_executor.h"

namespace nx::cloud::db::client {

AsyncRequestsExecutor::AsyncRequestsExecutor(
    network::cloud::CloudModuleUrlFetcher* const cdbEndPointFetcher)
    :
    m_cdbEndPointFetcher(
        std::make_unique<network::cloud::CloudModuleUrlFetcher::ScopedOperation>(
            cdbEndPointFetcher)),
    m_requestTimeout(nx::network::http::AsyncClient::Timeouts::defaults().responseReadTimeout)
{
}

AsyncRequestsExecutor::~AsyncRequestsExecutor()
{
    m_cdbEndPointFetcher.reset();

    pleaseStopSync();
}

void AsyncRequestsExecutor::setCredentials(nx::network::http::Credentials credentials)
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    m_auth.credentials = std::move(credentials);
}

void AsyncRequestsExecutor::setProxyCredentials(nx::network::http::Credentials credentials)
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    m_auth.proxyCredentials = std::move(credentials);
}

void AsyncRequestsExecutor::setProxyVia(
    const nx::network::SocketAddress& proxyEndpoint,
    nx::network::ssl::AdapterFunc adapterFunc,
    bool isSecure)
{
    NX_ASSERT(proxyEndpoint.port > 0);

    NX_MUTEX_LOCKER lk(&m_mutex);
    m_auth.proxyEndpoint = proxyEndpoint;
    m_auth.isProxySecure = isSecure;
    m_proxyAdapterFunc = std::move(adapterFunc);
}

void AsyncRequestsExecutor::setRequestTimeout(
    std::chrono::milliseconds timeout)
{
    m_requestTimeout = timeout;
}

std::chrono::milliseconds AsyncRequestsExecutor::requestTimeout() const
{
    return m_requestTimeout;
}

void AsyncRequestsExecutor::setAdditionalHeaders(nx::network::http::HttpHeaders headers)
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    m_additionalHeaders = std::move(headers);
}

nx::network::http::HttpHeaders AsyncRequestsExecutor::additionalHeaders() const
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    return m_additionalHeaders;
}

void AsyncRequestsExecutor::bindToAioThread(
    network::aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    for (auto& request: m_runningRequests)
        request->bindToAioThread(aioThread);
}

void AsyncRequestsExecutor::stopWhileInAioThread()
{
    m_runningRequests.clear();
}

} // namespace nx::cloud::db::client
