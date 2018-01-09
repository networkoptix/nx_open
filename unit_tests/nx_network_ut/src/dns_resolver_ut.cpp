#include <gtest/gtest.h>

#include <nx/network/address_resolver.h>
#include <nx/network/dns_resolver.h>
#include <nx/network/resolve/custom_resolver.h>
#include <nx/network/socket_global.h>
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
        m_expectedResponseCount(0),
        m_timeShifted(false)
    {
        using namespace std::placeholders;

        m_dnsResolver.registerResolver(
            makeCustomResolver(std::bind(&DnsResolver::testResolve, this, _1, _2, _3)),
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

    void assertIfRegularTaskDidNotTimeOut()
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
    bool m_timeShifted;

    SystemError::ErrorCode testResolve(
        const QString& hostName,
        int /*ipVersion*/,
        std::deque<HostAddress>* resolvedAddress)
    {
        if (hostName == hardToResolveHost)
        {
            addRegularTask();
            shiftTimeIfNeeded(); //< Emulating delay in this method.
        }

        if (!m_pipelinedRequests.empty())
            startNextPipelinedTask();

        resolvedAddress->push_back({HostAddress::localhost});
        return SystemError::noError;
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

    void shiftTimeIfNeeded()
    {
        using namespace nx::utils::test;

        QnMutexLocker lock(&m_mutex);
        if (m_timeShifted)
            return;

        m_shiftedTime.push_back(ScopedTimeShift(
            ClockType::steady,
            m_dnsResolver.resolveTimeout() + std::chrono::seconds(1)));
        m_timeShifted = true;
    }
};

TEST_F(DnsResolver, resolve_fails_with_timeout)
{
    addTaskThatTakesMoreThanResolveTimeout();
    // Regular task will be added when executing "long" task.
    assertIfRegularTaskDidNotTimeOut();
}

TEST_F(DnsResolver, pipelining_resolve_request)
{
    pipelineRequests();
    assertIfNotAllRequestsHaveBeenCompleted();
}

TEST_F(DnsResolver, old_and_unclear_test_to_be_refactored)
{
    auto& dnsResolver = SocketGlobals::addressResolver().dnsResolver();
    {
        std::deque<HostAddress> ips;
        const auto resultCode = dnsResolver.resolveSync(QLatin1String("ya.ru"), AF_INET, &ips);
        ASSERT_EQ(SystemError::noError, resultCode);
        ASSERT_GE(ips.size(), 1U);
        ASSERT_TRUE(ips.front().isIpAddress());
        ASSERT_TRUE((bool)ips.front().ipV4());
        ASSERT_TRUE((bool)ips.front().ipV6().first);
        ASSERT_NE(0U, ips.front().ipV4()->s_addr);
    }

    {
        std::deque<HostAddress> ips;
        const auto resultCode = dnsResolver.resolveSync(QLatin1String("hren2349jf234.ru"), AF_INET, &ips);
        ASSERT_EQ(SystemError::hostNotFound, resultCode);
        ASSERT_EQ(0U, ips.size());
    }

    {
        const QString kTestHost = QLatin1String("some-test-host-543242145.com");
        std::vector<HostAddress> kTestAddresses;
        kTestAddresses.push_back(*HostAddress::ipV4from("12.34.56.78"));
        kTestAddresses.push_back(*HostAddress::ipV6from("1234::abcd").first);

        dnsResolver.addEtcHost(kTestHost, kTestAddresses);

        std::deque<HostAddress> ip4s;
        SystemError::ErrorCode resultCode = dnsResolver.resolveSync(kTestHost, AF_INET, &ip4s);
        ASSERT_EQ(SystemError::noError, resultCode);

        std::deque<HostAddress> ip6s;
        resultCode = dnsResolver.resolveSync(kTestHost, AF_INET6, &ip6s);
        ASSERT_EQ(SystemError::noError, resultCode);

        dnsResolver.removeEtcHost(kTestHost);

        ASSERT_EQ(1U, ip4s.size());
        ASSERT_EQ(kTestAddresses.front(), ip4s.front());
        ASSERT_EQ(2U, ip6s.size());
        ASSERT_EQ(kTestAddresses.front(), ip6s.front());
        ASSERT_EQ(kTestAddresses.back(), ip6s.back());
    }
}

} // namespace test
} // namespace network
} // namespace nx
