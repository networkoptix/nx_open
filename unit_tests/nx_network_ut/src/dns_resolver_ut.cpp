#include <gtest/gtest.h>

#include <nx/network/dns_resolver.h>
#include <nx/network/resolve/custom_resolver.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>
#include <nx/utils/time.h>

namespace nx {
namespace network {
namespace test {

static const QString hardToResolveHost("takes_more_than_timeout_to_resolve");
static const QString easyToResolveHost("resolves_immediately");

class DnsResolver:
    public ::testing::Test
{
public:
    DnsResolver():
        m_prevResolveRequestId(0),
        m_expectedResponseCount(0)
    {
        using namespace std::placeholders;

        m_dnsResolver.registerResolver(
            makeCustomResolver(std::bind(&DnsResolver::testResolve, this, _1, _2)),
            m_dnsResolver.maxRegisteredResolverPriority() + 1);
    }

    ~DnsResolver()
    {
    }

protected:
    void addTaskThatTakesMoreThanResolveTimeout()
    {
        startResolveAsync(hardToResolveHost);
    }

    void addRegularTask()
    {
        startResolveAsync(easyToResolveHost);
    }

    void assertIfRegularTaskDidNotTimedOut()
    {
        QnMutexLocker lock(&m_mutex);
        waitForResolveResult(&lock, easyToResolveHost);
        ASSERT_EQ(SystemError::timedOut, m_hostNameToResolveResult[easyToResolveHost]);
    }

    void pipelineRequests()
    {
        m_pipelinedRequests.push_back(easyToResolveHost + "1");
        m_pipelinedRequests.push_back(easyToResolveHost + "2");
        m_pipelinedRequests.push_back(easyToResolveHost + "3");
        m_expectedResponseCount = m_pipelinedRequests.size();
        startNextPipelinedTask();
    }

    void assertIfNotAllRequestsHaveBeenCompleted()
    {
        QnMutexLocker lock(&m_mutex);
        waitForResolveResult(&lock, m_expectedResponseCount);
        ASSERT_EQ(m_expectedResponseCount, m_hostNameToResolveResult.size());
        ASSERT_EQ(m_expectedResponseCount, m_requestIdToResolveResult.size());
    }

private:
    network::DnsResolver m_dnsResolver;
    std::atomic<std::size_t> m_prevResolveRequestId;
    std::size_t m_expectedResponseCount;
    QnMutex m_mutex;
    std::map<QString, SystemError::ErrorCode> m_hostNameToResolveResult;
    std::map<std::size_t, SystemError::ErrorCode> m_requestIdToResolveResult;
    QnWaitCondition m_cond;
    std::list<nx::utils::test::ScopedTimeShift> m_shiftedTime;
    std::list<QString> m_pipelinedRequests;

    std::deque<HostAddress> testResolve(const QString& hostName, int /*ipVersion*/)
    {
        using namespace nx::utils::test;

        if (hostName == hardToResolveHost)
        {
            // Shifting time instead of sleep.
            m_shiftedTime.push_back(ScopedTimeShift(
                ClockType::steady,
                m_dnsResolver.resolveTimeout() + std::chrono::seconds(1)));
        }

        if (!m_pipelinedRequests.empty())
            startNextPipelinedTask();

        return std::deque<HostAddress>( {HostAddress::localhost} );
    }

    void startNextPipelinedTask()
    {
        auto hostname = std::move(m_pipelinedRequests.front());
        m_pipelinedRequests.pop_front();
        startResolveAsync(hostname);
    }

    void onHostResolved(
        const QString& hostName,
        std::size_t requestId,
        SystemError::ErrorCode errorCode,
        std::deque<HostAddress> /*resolvedAddresses*/)
    {
        QnMutexLocker lock(&m_mutex);
        m_hostNameToResolveResult.emplace(hostName, errorCode);
        m_requestIdToResolveResult.emplace(requestId, errorCode);
        m_cond.wakeAll();
    }

    void startResolveAsync(const QString& hostName)
    {
        using namespace std::placeholders;

        const auto requestId = ++m_prevResolveRequestId;
        m_dnsResolver.resolveAsync(
            hostName,
            std::bind(&DnsResolver::onHostResolved, this, hostName, requestId, _1, _2),
            AF_INET,
            reinterpret_cast<network::DnsResolver::RequestId>(requestId));
    }

    void waitForResolveResult(QnMutexLockerBase* const lock, const QString& hostName)
    {
        while (m_hostNameToResolveResult.find(hostName) == m_hostNameToResolveResult.end())
            m_cond.wait(lock->mutex());
    }

    void waitForResolveResult(QnMutexLockerBase* const lock, std::size_t requestId)
    {
        while (m_requestIdToResolveResult.find(requestId) == m_requestIdToResolveResult.end())
            m_cond.wait(lock->mutex());
    }
};

TEST_F(DnsResolver, resolve_fails_with_timeout)
{
    addTaskThatTakesMoreThanResolveTimeout();
    addRegularTask();
    assertIfRegularTaskDidNotTimedOut();
}

TEST_F(DnsResolver, pipelining_resolve_request)
{
    pipelineRequests();
    assertIfNotAllRequestsHaveBeenCompleted();
}

} // namespace test
} // namespace network
} // namespace nx
