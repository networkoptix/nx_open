#include "async_http_requests_executor.h"

#include <nx/network/deprecated/asynchttpclient.h>

namespace nx::cloud::db::client {

AsyncRequestsExecutor::AsyncRequestsExecutor(
    network::cloud::CloudModuleUrlFetcher* const cdbEndPointFetcher)
    :
    m_cdbEndPointFetcher(
        std::make_unique<network::cloud::CloudModuleUrlFetcher::ScopedOperation>(
            cdbEndPointFetcher)),
    m_requestTimeout(nx::network::http::AsyncHttpClient::Timeouts::kDefaultResponseReadTimeout)
{
}

AsyncRequestsExecutor::~AsyncRequestsExecutor()
{
    m_cdbEndPointFetcher.reset();

    pleaseStopSync();
}

void AsyncRequestsExecutor::setCredentials(
    const std::string& login,
    const std::string& password)
{
    QnMutexLocker lk(&m_mutex);
    m_auth.user.username = QString::fromStdString(login);
    m_auth.user.authToken.setPassword(password.c_str());
}

void AsyncRequestsExecutor::setProxyCredentials(
    const std::string& login,
    const std::string& password)
{
    QnMutexLocker lk(&m_mutex);
    m_auth.proxyUser.username = QString::fromStdString(login);
    m_auth.proxyUser.authToken.setPassword(password.c_str());
}

void AsyncRequestsExecutor::setProxyVia(
    const nx::network::SocketAddress& proxyEndpoint, bool isSecure)
{
    NX_ASSERT(proxyEndpoint.port > 0);

    QnMutexLocker lk(&m_mutex);
    m_auth.proxyEndpoint = proxyEndpoint;
    m_auth.isProxySecure = isSecure;
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
