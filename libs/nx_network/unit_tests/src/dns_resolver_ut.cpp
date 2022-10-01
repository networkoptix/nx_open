// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <list>

#include <gtest/gtest.h>

#include <nx/network/address_resolver.h>
#include <nx/network/dns_resolver.h>
#include <nx/network/resolve/custom_resolver.h>
#include <nx/network/socket_global.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/string.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>
#include <nx/utils/time.h>

namespace nx::network::test {

static constexpr char hardToResolveHost[] = "takes_more_than_timeout_to_resolve";
static constexpr char easyToResolveHost[] = "resolved_immediately";

class DnsResolver:
    public ::testing::Test
{
public:
    DnsResolver()
    {
        m_dnsResolver.registerResolver(
            makeCustomResolver([this](auto&&... args) {
                return testResolve(std::forward<decltype(args)>(args)...);
            }),
            m_dnsResolver.maxRegisteredResolverPriority() + 1);
    }

    ~DnsResolver()
    {
        m_dnsResolver.stop();
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
        NX_MUTEX_LOCKER lock(&m_mutex);
        waitForResolveResult(&lock, easyToResolveHost);
        ASSERT_EQ(SystemError::timedOut, m_hostNameToResolveResult[easyToResolveHost]);
    }

    void pipelineRequests()
    {
        m_pipelinedRequests.push_back(nx::utils::buildString(easyToResolveHost, '1'));
        m_pipelinedRequests.push_back(nx::utils::buildString(easyToResolveHost, '2'));
        m_pipelinedRequests.push_back(nx::utils::buildString(easyToResolveHost, '3'));
        m_expectedResponseCount = m_pipelinedRequests.size();
        startNextPipelinedTask();
    }

    void assertIfNotAllRequestsHaveBeenCompleted()
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        waitForResolveResult(&lock, m_expectedResponseCount);
        ASSERT_EQ(m_expectedResponseCount, m_hostNameToResolveResult.size());
        ASSERT_EQ(m_expectedResponseCount, m_requestIdToResolveResult.size());
    }

private:
    network::DnsResolver m_dnsResolver;
    std::atomic<std::size_t> m_prevResolveRequestId{0};
    std::size_t m_expectedResponseCount = 0;
    nx::Mutex m_mutex;
    std::map<std::string, SystemError::ErrorCode> m_hostNameToResolveResult;
    std::map<std::size_t, SystemError::ErrorCode> m_requestIdToResolveResult;
    nx::WaitCondition m_cond;
    std::list<nx::utils::test::ScopedTimeShift> m_shiftedTime;
    std::list<std::string> m_pipelinedRequests;
    bool m_timeShifted = false;

    SystemError::ErrorCode testResolve(
        const std::string_view& hostName,
        int /*ipVersion*/,
        ResolveResult* resolved)
    {
        if (hostName == hardToResolveHost)
        {
            addRegularTask();
            shiftTimeIfNeeded(); //< Emulating delay in this method.
        }

        if (!m_pipelinedRequests.empty())
            startNextPipelinedTask();

        resolved->entries.push_back({AddressType::direct, HostAddress::localhost});
        return SystemError::noError;
    }

    void startNextPipelinedTask()
    {
        auto hostname = std::move(m_pipelinedRequests.front());
        m_pipelinedRequests.pop_front();
        startResolveAsync(hostname);
    }

    void onHostResolved(
        const std::string& hostName,
        std::size_t requestId,
        SystemError::ErrorCode errorCode,
        std::deque<HostAddress> /*resolvedAddresses*/)
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        m_hostNameToResolveResult.emplace(hostName, errorCode);
        m_requestIdToResolveResult.emplace(requestId, errorCode);
        m_cond.wakeAll();
    }

    void startResolveAsync(const std::string& hostName)
    {
        using namespace std::placeholders;

        const auto requestId = ++m_prevResolveRequestId;
        m_dnsResolver.resolveAsync(
            hostName,
            std::bind(&DnsResolver::onHostResolved, this, hostName, requestId, _1, _2),
            AF_INET,
            reinterpret_cast<network::DnsResolver::RequestId>(requestId));
    }

    void waitForResolveResult(nx::Locker<nx::Mutex>* const lock, const std::string& hostName)
    {
        while (m_hostNameToResolveResult.find(hostName) == m_hostNameToResolveResult.end())
            m_cond.wait(lock->mutex());
    }

    void waitForResolveResult(nx::Locker<nx::Mutex>* const lock, std::size_t requestId)
    {
        while (m_requestIdToResolveResult.find(requestId) == m_requestIdToResolveResult.end())
            m_cond.wait(lock->mutex());
    }

    void shiftTimeIfNeeded()
    {
        using namespace nx::utils::test;

        NX_MUTEX_LOCKER lock(&m_mutex);
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
        ResolveResult resolved;
        const auto resultCode = dnsResolver.resolveSync("localhost", AF_INET, &resolved);
        ASSERT_EQ(SystemError::noError, resultCode);
        ASSERT_GE(resolved.entries.size(), 1U);
        ASSERT_TRUE(resolved.entries.front().host.isIpAddress());
        ASSERT_TRUE((bool)resolved.entries.front().host.ipV4());
        ASSERT_TRUE((bool)resolved.entries.front().host.ipV6().first);
        ASSERT_NE(0U, resolved.entries.front().host.ipV4()->s_addr);
    }

    {
        ResolveResult resolved;
        const auto resultCode = dnsResolver.resolveSync(
            "unknown-894ae3a4-5a95-42c3-a674-dd0e1c16e415.ru.", AF_INET, &resolved);
        ASSERT_EQ(SystemError::hostNotFound, resultCode);
        ASSERT_EQ(0U, resolved.entries.size());
    }

    {
        const std::string kTestHost("test-3a7b4472-8393-4a2e-94f1-d657a5b7eb1d.com");
        std::vector<HostAddress> kTestAddresses;
        kTestAddresses.push_back(*HostAddress::ipV4from("12.34.56.78"));
        kTestAddresses.push_back(*HostAddress::ipV6from("1234::abcd").first);

        dnsResolver.addEtcHost(kTestHost, kTestAddresses);

        ResolveResult resolvedV4;
        SystemError::ErrorCode resultCode = dnsResolver.resolveSync(kTestHost, AF_INET, &resolvedV4);
        ASSERT_EQ(SystemError::noError, resultCode);

        ResolveResult resolvedV6;
        resultCode = dnsResolver.resolveSync(kTestHost, AF_INET6, &resolvedV6);
        ASSERT_EQ(SystemError::noError, resultCode);

        dnsResolver.removeEtcHost(kTestHost);

        ASSERT_EQ(1U, resolvedV4.entries.size());
        ASSERT_EQ(kTestAddresses.front(), resolvedV4.entries.front().host);

        ASSERT_EQ(2U, resolvedV6.entries.size());
        ASSERT_EQ(kTestAddresses.front(), resolvedV6.entries.front().host);
        ASSERT_EQ(kTestAddresses.back(), resolvedV6.entries.back().host);
    }
}

} // namespace nx::network::test
