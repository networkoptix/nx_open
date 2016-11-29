#include <gtest/gtest.h>

#include <nx/network/dns_resolver.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>

namespace nx {
namespace network {
namespace test {

static const QString hardToResolveHost("takes_more_than_timeout_to_resolve");
static const QString easyToResolveHost("resolves_immediately");

template<typename Func>
class CustomResolver:
    public AbstractResolver
{
public:
    CustomResolver(Func func):
        m_func(std::move(func))
    {
    }

    virtual std::deque<HostAddress> resolve(const QString& hostName, int ipVersion) override
    {
        return m_func(hostName, ipVersion);
    }

private:
    Func m_func;
};

template<typename Func>
static std::unique_ptr<CustomResolver<Func>> makeCustomResolver(Func func)
{
    return std::make_unique<CustomResolver<Func>>(std::move(func));
}

class FtDnsResolver:
    public ::testing::Test
{
public:
    FtDnsResolver():
        m_prevResolveRequestId(0)
    {
        using namespace std::placeholders;

        m_dnsResolver.setResolveTimeout(std::chrono::seconds(1));
        m_dnsResolver.registerResolver(
            makeCustomResolver(std::bind(&FtDnsResolver::testResolve, this, _1, _2)),
            m_dnsResolver.maxRegisteredResolverPriority() + 1);
    }

    ~FtDnsResolver()
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

private:
    network::DnsResolver m_dnsResolver;
    std::size_t m_prevResolveRequestId;
    QnMutex m_mutex;
    std::map<QString, SystemError::ErrorCode> m_hostNameToResolveResult;
    QnWaitCondition m_cond;

    std::deque<HostAddress> testResolve(const QString& hostName, int /*ipVersion*/)
    {
        if (hostName == hardToResolveHost)
            std::this_thread::sleep_for(m_dnsResolver.resolveTimeout() + std::chrono::seconds(1));
        return std::deque<HostAddress>( {HostAddress::localhost} );
    }

    void startResolveAsync(const QString& hostName)
    {
        using namespace std::placeholders;

        m_dnsResolver.resolveAsync(
            hostName,
            std::bind(&FtDnsResolver::onHostResolved, this, hostName, _1, _2),
            AF_INET,
            reinterpret_cast<network::DnsResolver::RequestId>(++m_prevResolveRequestId));
    }

    void onHostResolved(
        const QString& hostName,
        SystemError::ErrorCode errorCode,
        std::deque<HostAddress> resolvedAddresses)
    {
        QnMutexLocker lock(&m_mutex);
        m_hostNameToResolveResult.emplace(hostName, errorCode);
        m_cond.wakeAll();
    }

    void waitForResolveResult(QnMutexLockerBase* const lock, const QString& hostName)
    {
        while (m_hostNameToResolveResult.find(hostName) == m_hostNameToResolveResult.end())
            m_cond.wait(lock->mutex());
    }
};

TEST_F(FtDnsResolver, resolve_fails_with_timeout)
{
    addTaskThatTakesMoreThanResolveTimeout();
    addRegularTask();
    assertIfRegularTaskDidNotTimedOut();
}

} // namespace test
} // namespace network
} // namespace nx
