#include "async_http_requests_executor.h"

#include <nx/network/http/asynchttpclient.h>

namespace nx {
namespace cdb {
namespace client {

AsyncRequestsExecutor::AsyncRequestsExecutor(
    network::cloud::CloudModuleUrlFetcher* const cdbEndPointFetcher)
    :
    m_cdbEndPointFetcher(
        std::make_unique<network::cloud::CloudModuleUrlFetcher::ScopedOperation>(
            cdbEndPointFetcher)),
    m_requestTimeout(nx_http::AsyncHttpClient::Timeouts::kDefaultResponseReadTimeout)
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
    m_auth.username = QString::fromStdString(login);
    m_auth.password = QString::fromStdString(password);
}

void AsyncRequestsExecutor::setProxyCredentials(
    const std::string& login,
    const std::string& password)
{
    QnMutexLocker lk(&m_mutex);
    m_auth.proxyUsername = QString::fromStdString(login);
    m_auth.proxyPassword = QString::fromStdString(password);
}

void AsyncRequestsExecutor::setProxyVia(const SocketAddress& proxyEndpoint)
{
    NX_ASSERT(proxyEndpoint.port > 0);

    QnMutexLocker lk(&m_mutex);
    m_auth.proxyEndpoint = proxyEndpoint;
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

} // namespace client
} // namespace cdb
} // namespace nx
