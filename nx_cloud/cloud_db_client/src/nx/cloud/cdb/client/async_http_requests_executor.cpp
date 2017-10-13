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
    QnMutexLocker lk(&m_mutex);
    while (!m_runningRequests.empty())
    {
        auto request = std::move(m_runningRequests.front());
        m_runningRequests.pop_front();
        lk.unlock();
        request->pleaseStopSync();
        request.reset();
        lk.relock();
    }
}

void AsyncRequestsExecutor::setCredentials(
    const std::string& login,
    const std::string& password)
{
    QnMutexLocker lk(&m_mutex);
    m_auth.user.username = QString::fromStdString(login);
    m_auth.user.authToken.setPassword(QString::fromStdString(password));
}

void AsyncRequestsExecutor::setProxyCredentials(
    const std::string& login,
    const std::string& password)
{
    QnMutexLocker lk(&m_mutex);
    m_auth.proxyUser.username = QString::fromStdString(login);
    m_auth.proxyUser.authToken.setPassword(QString::fromStdString(password));
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

} // namespace client
} // namespace cdb
} // namespace nx
