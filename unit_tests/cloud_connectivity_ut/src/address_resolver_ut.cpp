#include <thread>

#include <gtest/gtest.h>

#include <nx/network/address_resolver.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/cloud/mediator_connector.h>
#include <nx/network/socket_global.h>
#include <nx/utils/test_support/sync_queue.h>

namespace nx {
namespace network {
namespace cloud {
namespace test {

#if 1

class AddressResolver:
    public ::testing::Test
{
};

/**
 * For every address expecting following resolve order:
 * - Fixed address table.
 * - cloud resolve, if and only if hostname looks like cloud address ("guid" or "guid.guid").
 * - Regular DNS.
 */
class AddressResolverOrder:
    public AddressResolver
{
public:
    AddressResolverOrder():
        m_addressResolver(SocketGlobals::addressResolver())
    {
        SocketGlobals::cloud().reinitialize();
        SocketGlobals::cloud().mediatorConnector().mockupMediatorUrl(
            nx::utils::Url("stun://127.0.0.1:12345"));
    }

    ~AddressResolverOrder()
    {
        SocketGlobals::cloud().reinitialize();
    }

protected:
    void givenCloudHostname()
    {
        m_hostname = QnUuid::createUuid().toSimpleString();
    }

    void givenNonCloudHostname()
    {
        m_hostname = "example.com";
    }

    void addFixedEntry()
    {
        m_fixedEntry = SocketAddress(HostAddress::localhost, 12345);
        m_addressResolver.addFixedAddress(
            m_hostname,
            m_fixedEntry);
    }

    void makeHostnameResolvableWithDns()
    {
        m_dnsEntry = HostAddress::localhost;
        m_addressResolver.dnsResolver().addEtcHost(
            m_hostname,
            {{m_dnsEntry}});
    }

    void disableCloudConnect()
    {
        SocketGlobals::cloud().reinitialize();
        // Mediator is not resolved, so cloud connect is disabled.
    }

    void assertResolveReturnsCloudEntry()
    {
        resolve();
        assertResolveSucceeded();

        ASSERT_EQ(1U, m_resolvedEntries.size());
        ASSERT_EQ(AddressType::cloud, m_resolvedEntries.front().type);
        ASSERT_EQ(m_hostname, m_resolvedEntries.front().host.toString());
    }

    void assertResolveReturnsDnsResolveResult()
    {
        resolve();
        assertResolveSucceeded();

        ASSERT_EQ(1U, m_resolvedEntries.size());
        ASSERT_EQ(AddressType::direct, m_resolvedEntries.front().type);
        ASSERT_EQ(m_dnsEntry, m_resolvedEntries.front().host);
    }

    void assertResolveReturnsFixedEntry()
    {
        resolve();
        assertResolveSucceeded();

        ASSERT_EQ(1U, m_resolvedEntries.size());
        ASSERT_EQ(AddressType::direct, m_resolvedEntries.front().type);
        ASSERT_EQ(m_fixedEntry, m_resolvedEntries.front().toEndpoint());
    }

    void assertHostnameCannotBeResolved()
    {
        resolve();
        assertResolveFailed();
    }

    void assertResolveSucceeded()
    {
        ASSERT_EQ(SystemError::noError, m_resolveResult);
    }

    void assertResolveFailed()
    {
        ASSERT_NE(SystemError::noError, m_resolveResult);
    }

private:
    nx::network::AddressResolver& m_addressResolver;
    QString m_hostname;
    SocketAddress m_fixedEntry;
    HostAddress m_dnsEntry;
    SystemError::ErrorCode m_resolveResult = SystemError::noError;
    std::deque<AddressEntry> m_resolvedEntries;

    void resolve()
    {
        m_resolvedEntries = m_addressResolver.resolveSync(
            m_hostname, NatTraversalSupport::enabled, AF_INET);
        if (m_resolvedEntries.empty())
            m_resolveResult = SystemError::getLastOSErrorCode();
    }
};

TEST_F(AddressResolverOrder, fixed_resolve_precedes_cloud_resolve)
{
    givenCloudHostname();
    addFixedEntry();
    assertResolveReturnsFixedEntry();
}

TEST_F(AddressResolverOrder, cloud_resolve_precedes_dns_resolve)
{
    givenCloudHostname();
    makeHostnameResolvableWithDns();
    assertResolveReturnsCloudEntry();
}

TEST_F(AddressResolverOrder, cloud_resolve_is_skipped_for_non_cloud_address)
{
    givenNonCloudHostname();
    makeHostnameResolvableWithDns();
    assertResolveReturnsDnsResolveResult();
}

TEST_F(
    AddressResolverOrder,
    cloud_resolve_is_skipped_if_cloud_connect_is_not_initialized)
{
    disableCloudConnect();

    givenCloudHostname();
    makeHostnameResolvableWithDns();
    assertResolveReturnsDnsResolveResult();
}

TEST_F(
    AddressResolverOrder,
    cloud_hostname_is_not_resolved_if_cloud_connect_is_not_initialized)
{
    disableCloudConnect();

    givenCloudHostname();
    assertHostnameCannotBeResolved();
}

#else

class AddressResolverCloudResolving:
    public ::testing::Test,
    protected AddressResolver
{
public:
    static void emulateAddress(
        const nx::network::HostAddress& address,
        std::list<nx::network::SocketAddress> endpoints = {})
    {
        if (endpoints.size())
            s_endpoints.emplace(address.toString(), std::move(endpoints));
        else
            s_endpoints.erase(address.toString());
    }

    static void TearDownTestCase()
    {
        s_endpoints.clear();
    }

    AddressResolverCloudResolving()
    {
        setCloudResolveEnabled(true);
    }

    virtual ~AddressResolverCloudResolving() override
    {
        pleaseStopSync();
    }

    void resolveAndCheckState(
        const nx::network::HostAddress& address,
        utils::MoveOnlyFunc<void(HaInfoIterator it)> checker)
    {
        NX_LOGX(lm("resolveAndCheckState %1").arg(address), cl_logDEBUG1);
        nx::utils::TestSyncQueue<bool> syncQueue;
        static const size_t kSimultaneousQueries = 100;
        for (size_t counter = kSimultaneousQueries; counter; --counter)
        {
            resolveAsync(
                address,
                [&](SystemError::ErrorCode /*code*/, std::deque<AddressEntry> /*enries*/)
                {
                    {
                        QnMutexLocker lk(&m_mutex);
                        checker(m_info.find(address));
                    }

                    syncQueue.push(true);
                },
                NatTraversalSupport::enabled,
                AF_INET);
        }

        for (size_t counter = kSimultaneousQueries; counter; --counter)
            syncQueue.pop();
    }

    void resolveAndCheckStateWithSub(
        const nx::network::HostAddress& address,
        const std::function<void(HaInfoIterator it, bool isSub)>& checker)
    {
        resolveAndCheckState(address, std::bind2nd(checker, false));
        const nx::network::HostAddress sub(address.toString().split('.')[1]);
        resolveAndCheckState(sub, std::bind2nd(checker, true));
    }

private:
    static std::map<QString, std::list<nx::network::SocketAddress>> s_endpoints;
};

std::map<QString, std::list<nx::network::SocketAddress>> AddressResolverCloudResolving::s_endpoints;

static const nx::network::HostAddress kAddress("ya.ru");
static const nx::network::SocketAddress kResult(*nx::network::HostAddress::ipV4from(lit("10.11.12.13")), 12345);

/**
 * Usual DNS addresses like "ya.ru" shell be resolved in order:
 *  fixed -> dns -> mediator (never touches mediator because of valid DNS)
 */
TEST_F(AddressResolverCloudResolving, FixedVsDns)
{
    addFixedAddress(kAddress, kResult);
    resolveAndCheckState(
        kAddress, [&](HaInfoIterator it)
        {
            const HostAddressInfo& info = it->second;
            ASSERT_EQ(info.fixedEntries.size(), 1U);

            const AddressEntry& entry = info.fixedEntries.front();
            EXPECT_EQ(entry.type, AddressType::direct);
            EXPECT_EQ(entry.host, kResult.address);
            EXPECT_EQ(entry.attributes.size(), 1U);
            EXPECT_EQ(entry.attributes.front().type, AddressAttributeType::port);
            EXPECT_EQ(entry.attributes.front().value, kResult.port);

            EXPECT_EQ(info.dnsState(), HostAddressInfo::State::unresolved);
            EXPECT_EQ(info.mediatorState(), HostAddressInfo::State::unresolved);
        });

    size_t yandexIpCount = 0;
    removeFixedAddress(kAddress, kResult);
    resolveAndCheckState(
        kAddress, [&](HaInfoIterator it)
        {
            const HostAddressInfo& info = it->second;
            EXPECT_EQ(info.fixedEntries.size(), 0U);
            EXPECT_EQ(info.dnsState(), HostAddressInfo::State::resolved);
            EXPECT_EQ(info.mediatorState(), HostAddressInfo::State::unresolved);

            const auto entries = info.getAll();
            yandexIpCount = entries.size();
            EXPECT_GT(yandexIpCount, 0U);
            for (const auto& e : entries)
                EXPECT_EQ(e.type, AddressType::direct);
        });

    addFixedAddress(kAddress, kResult);
    resolveAndCheckState(
        kAddress, [&](HaInfoIterator it)
        {
            const HostAddressInfo& info = it->second;
            ASSERT_EQ(info.fixedEntries.size(), 1U);

            const AddressEntry& entry = info.fixedEntries.front();
            EXPECT_EQ(entry.type, AddressType::direct);
            EXPECT_EQ(entry.host, kResult.address);
            ASSERT_EQ(entry.attributes.size(), 1U);
            EXPECT_EQ(entry.attributes.front().type, AddressAttributeType::port);
            EXPECT_EQ(entry.attributes.front().value, kResult.port);

            EXPECT_EQ(info.dnsState(), HostAddressInfo::State::resolved);
            EXPECT_EQ(info.mediatorState(), HostAddressInfo::State::unresolved);
            EXPECT_EQ(info.getAll().size(), 1 + yandexIpCount); // still have them
        });
}

static QString testUuid()
{
    return QnUuid::createUuid().toSimpleString();
}

/**
 * Cloud-like adresses shell be resolved in next order:
 *  fixed -> mediator -> dns
 */
TEST_F(AddressResolverCloudResolving, FixedVsMediatorVsDns)
{
    const std::vector<nx::network::HostAddress> kGoodCloudAddresses =
    {
        nx::network::HostAddress(lm("server.%1").arg(testUuid())),
        nx::network::HostAddress(lm("%1.%2").arg(testUuid()).arg(testUuid())),
    };

    for (const auto& host : kGoodCloudAddresses)
    {
        emulateAddress(host, {kResult});
        resolveAndCheckStateWithSub(
            host, [&](HaInfoIterator it, bool isSub)
            {
                const HostAddressInfo& info = it->second;
                EXPECT_EQ(info.fixedEntries.size(), 0U);
                if (!isSub)
                {
                    EXPECT_EQ(info.dnsState(), HostAddressInfo::State::unresolved);
                }
                EXPECT_EQ(info.mediatorState(), HostAddressInfo::State::resolved);

                const auto entries = info.getAll();
                ASSERT_EQ(1U, info.getAll().size());

                const AddressEntry entry2 = entries.back();
                EXPECT_EQ(entry2.type, AddressType::cloud);
                EXPECT_EQ(entry2.host, it->first);
            });

        addFixedAddress(host, kResult);
        resolveAndCheckStateWithSub(
            host, [&](HaInfoIterator it, bool isSub)
            {
                const HostAddressInfo& info = it->second;
                ASSERT_EQ(info.fixedEntries.size(), 1U);

                const AddressEntry& entry = info.fixedEntries.front();
                EXPECT_EQ(entry.type, AddressType::direct);
                EXPECT_EQ(entry.host, kResult.address);
                ASSERT_EQ(entry.attributes.size(), 1U);
                EXPECT_EQ(entry.attributes.front().type, AddressAttributeType::port);
                EXPECT_EQ(entry.attributes.front().value, kResult.port);

                EXPECT_EQ(2U, info.getAll().size());
                if (!isSub)
                {
                    EXPECT_EQ(info.dnsState(), HostAddressInfo::State::unresolved);
                }
                EXPECT_EQ(info.mediatorState(), HostAddressInfo::State::resolved);
            });
    }

    const std::vector<nx::network::HostAddress> kBadCloudAddresses =
    {
        nx::network::HostAddress(lm("server.%1").arg(testUuid())),
        nx::network::HostAddress(lm("%1.%2").arg(testUuid()).arg(testUuid())),
    };

    for (const auto& host : kBadCloudAddresses)
    {
        resolveAndCheckStateWithSub(
            host, [&](HaInfoIterator it, bool)
            {
                typedef HostAddressInfo::State st;
                const HostAddressInfo& info = it->second;
                EXPECT_EQ(info.fixedEntries.size(), 0U);
                EXPECT_EQ(1U, info.getAll().size());
                EXPECT_EQ(st::unresolved, info.dnsState());
                EXPECT_EQ(info.mediatorState(), st::resolved);
            });
    }
}

/**
 * Usual DNS addresses like "ya.ru" shell be resolved in order:
 *  fixed -> dns -> mediator (always uses mediator because of not valid DNS)
 */
TEST_F(AddressResolverCloudResolving, DnsVsMediator)
{
    static const nx::network::HostAddress kAddressGood("hren-resolve-me-1.com");
    static const nx::network::HostAddress kAddressBad("hren-resolve-me-2.com");

    emulateAddress(kAddressGood, {kResult});
    resolveAndCheckState(
        kAddressGood, [&](HaInfoIterator it)
        {
            const HostAddressInfo& info = it->second;
            EXPECT_EQ(info.fixedEntries.size(), 0U);
            EXPECT_EQ(info.dnsState(), HostAddressInfo::State::resolved);
            EXPECT_EQ(info.mediatorState(), HostAddressInfo::State::resolved);

            const auto entries = info.getAll();
            ASSERT_EQ(0U, info.getAll().size());
        });

    resolveAndCheckState(
        kAddressBad, [&](HaInfoIterator it)
        {
            const HostAddressInfo& info = it->second;
            EXPECT_EQ(info.fixedEntries.size(), 0U);
            EXPECT_EQ(info.getAll().size(), 0U);
            EXPECT_EQ(info.dnsState(), HostAddressInfo::State::resolved);
            EXPECT_EQ(info.mediatorState(), HostAddressInfo::State::resolved);
        });
}

TEST(AddressResolverRealTest, Cancel)
{
    const auto doNone = [&](SystemError::ErrorCode, std::deque<AddressEntry>) {};

    const std::vector<nx::network::HostAddress> kTestAddresses =
    {
        nx::network::HostAddress("ya.ru"),
        nx::network::HostAddress("hren-resolve-me-1.com"),
        nx::network::HostAddress(lm("%1.%2").arg(testUuid()).arg(testUuid())),
    };

    for (size_t timeout = 0; timeout <= 5000; timeout *= 2)
    {
        for (const auto& address : kTestAddresses)
        {
            SocketGlobals::addressResolver().resolveAsync(
                address, doNone, NatTraversalSupport::enabled, AF_INET, this);
            if (timeout)
                std::this_thread::sleep_for(std::chrono::milliseconds(timeout));
            SocketGlobals::addressResolver().cancel(this);
            if (!timeout)
                timeout = 10;
        }
    }
}

#endif

} // namespace test
} // namespace cloud
} // namespace network
} // namespace nx
