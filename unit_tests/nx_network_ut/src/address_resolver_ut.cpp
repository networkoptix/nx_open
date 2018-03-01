#include <memory>

#include <gtest/gtest.h>

#include <nx/network/aio/aio_service.h>
#include <nx/network/abstract_socket.h>
#include <nx/network/address_resolver.h>
#include <nx/network/dns_resolver.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/resolve/custom_resolver.h>
#include <nx/network/socket_global.h>
#include <nx/network/socket_factory.h>
#include <nx/network/system_socket.h>
#include <nx/utils/random.h>
#include <nx/utils/std/future.h>
#include <nx/utils/thread/sync_queue.h>

namespace nx {
namespace network {
namespace test {

namespace {

class AddressResolverDeprecatedTests:
    public testing::Test
{
public:
    AddressResolverDeprecatedTests():
        m_startedConnectionsCount(0),
        m_completedConnectionsCount(0)
    {
    }

protected:
    std::mutex m_mutex;
    std::atomic<int> m_startedConnectionsCount;
    std::atomic<int> m_completedConnectionsCount;
    std::deque<std::unique_ptr<AbstractStreamSocket>> m_connections;
    nx::network::http::TestHttpServer m_testServer;
    SocketAddress m_serverEndpoint;

    virtual void SetUp() override
    {
        ASSERT_TRUE(m_testServer.bindAndListen(SocketAddress::anyPrivateAddress))
            << SystemError::getLastOSErrorText().toStdString();

        m_serverEndpoint.address =
            nx::utils::generateRandomName(7).toStdString();
        m_serverEndpoint.port = m_testServer.serverAddress().port;
        SocketGlobals::addressResolver().dnsResolver().addEtcHost(
            m_serverEndpoint.address.toString(),
            {{ m_testServer.serverAddress().address }});
    }

    void startAnotherSocketNonSafe()
    {
        using namespace std::placeholders;

        auto connection = SocketFactory::createStreamSocket();
        ASSERT_TRUE(connection->setNonBlockingMode(true));
        AbstractStreamSocket* connectionPtr = connection.get();
        m_connections.push_back(std::move(connection));
        connectionPtr->connectAsync(
            m_serverEndpoint,
            std::bind(&AddressResolverDeprecatedTests::onConnectionComplete, this,
                connectionPtr, _1));
    }

    void onConnectionComplete(
        AbstractStreamSocket* connectionPtr,
        SystemError::ErrorCode errorCode)
    {
        ASSERT_EQ(SystemError::noError, errorCode)
            << SystemError::toString(errorCode).toStdString();

        std::unique_lock<std::mutex> lk(m_mutex);

        auto iterToRemove = std::remove_if(
            m_connections.begin(), m_connections.end(),
            [connectionPtr](const std::unique_ptr<AbstractStreamSocket>& elem)
            {
                return elem.get() == connectionPtr;
            });
        if (iterToRemove != m_connections.end())
        {
            ++m_completedConnectionsCount;
            m_connections.erase(iterToRemove, m_connections.end());
        }

        while (m_connections.size() < CONCURRENT_CONNECTIONS)
        {
            if ((++m_startedConnectionsCount) <= TOTAL_CONNECTIONS)
                startAnotherSocketNonSafe();
            else
                break;
        }
    }

    static const int CONCURRENT_CONNECTIONS = 17;
    static const int TOTAL_CONNECTIONS = 77;
};

} // namespace

// TODO: #ak The following test does not belong here.
// It actually checks how socket use AddressResolver, not how AddressResolver works.
// So, this test should belong to some stream socket acceptance test group.

TEST_F(AddressResolverDeprecatedTests, HostNameResolve2)
{
    if (SocketFactory::isStreamSocketTypeEnforced())
        return;

    HostAddress resolvedAddress;

    m_startedConnectionsCount = CONCURRENT_CONNECTIONS;
    {
        std::unique_lock<std::mutex> lk(m_mutex);
        for (int i = 0; i < CONCURRENT_CONNECTIONS; ++i)
            startAnotherSocketNonSafe();
    }

    int canCancelIndex = 0;
    int cancelledConnectionsCount = 0;
    std::unique_lock<std::mutex> lk(m_mutex);
    while ((m_completedConnectionsCount + cancelledConnectionsCount) < TOTAL_CONNECTIONS)
    {
        std::unique_ptr<AbstractStreamSocket> connectionToCancel;
        if (m_connections.size() > 1 && (canCancelIndex < m_startedConnectionsCount))
        {
            auto connectionToCancelIter = m_connections.begin();
            size_t index = nx::utils::random::number<size_t>(0, m_connections.size() - 1);
            std::advance(connectionToCancelIter, index);
            connectionToCancel = std::move(*connectionToCancelIter);
            m_connections.erase(connectionToCancelIter);
            canCancelIndex += /*index +*/ 1;
        }
        lk.unlock();
        if (connectionToCancel)
        {
            connectionToCancel->pleaseStopSync();
            connectionToCancel.reset();
            ++cancelledConnectionsCount;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        lk.lock();
    }

    QString ipStr = resolvedAddress.toString();
    static_cast<void>(ipStr);
    ASSERT_TRUE(m_connections.empty());
}

TEST_F(AddressResolverDeprecatedTests, cancellation)
{
    const auto doNone = [&](SystemError::ErrorCode, std::deque<AddressEntry>) {};

    const std::vector<nx::network::HostAddress> kTestAddresses =
    {
        nx::network::HostAddress("example.com"),
        nx::network::HostAddress("7C6BA51F-317E-461E-B47D-17A14CB751B0.com"),
        nx::network::HostAddress(lm("%1.%2").args(
            QnUuid::createUuid().toSimpleString(),
            QnUuid::createUuid().toSimpleString())),
    };

    for (size_t timeout = 0; timeout <= 500; timeout *= 2)
    {
        for (const auto& address: kTestAddresses)
        {
            SocketGlobals::addressResolver().resolveAsync(
                address, doNone, NatTraversalSupport::enabled, AF_INET, this);
            if (timeout)
                std::this_thread::sleep_for(std::chrono::milliseconds(timeout));
            SocketGlobals::addressResolver().cancel(this);
            if (!timeout)
                timeout = 1;
        }
    }
}

//-------------------------------------------------------------------------------------------------

class AddressResolver:
    public ::testing::Test
{
public:
    AddressResolver()
    {
        using namespace std::placeholders;

        m_resolver.dnsResolver().registerResolver(
            makeCustomResolver(std::bind(
                &AddressResolver::saveHostNameWithoutResolving, this, _1, _2, _3)),
            m_resolver.dnsResolver().maxRegisteredResolverPriority() + 1);
    }

protected:
    void setIpVersionToUse(int ipVersion)
    {
        m_ipVersionToUse = ipVersion;
    }

    void givenStringRepresentationOfIpv4AddressAsHostname()
    {
        m_hostNameToResolve = "1.2.3.4";
    }

    void emulateHostnameResolvableByDnsResolver()
    {
        struct in_addr inAddr;
        memset(&inAddr, 0, sizeof(inAddr));
        inAddr.s_addr = nx::utils::random::number<std::uint32_t>();
        m_dnsEntry = HostAddress(inAddr);
        m_resolver.dnsResolver().addEtcHost(m_hostNameToResolve, {*m_dnsEntry});
    }

    void whenResolve()
    {
        ResolveResult resolveResult;
        std::get<1>(resolveResult) = m_resolver.resolveSync(
            m_hostNameToResolve,
            NatTraversalSupport::disabled,
            m_ipVersionToUse);
        std::get<0>(resolveResult) = SystemError::getLastOSErrorCode();
        m_resolveResults.push(std::move(resolveResult));
    }

    void whenResolve(const QString& hostName)
    {
        m_hostNameToResolve = hostName;
        whenResolve();
    }

    void thenResolveSucceeded()
    {
        m_prevResolveResult = m_resolveResults.pop();
        ASSERT_EQ(SystemError::noError, std::get<0>(m_prevResolveResult));
    }

    void andResolvedTo(const HostAddress& hostAddress)
    {
        const auto& entries = std::get<1>(m_prevResolveResult);
        auto entryIter = std::find_if(
            entries.begin(), entries.end(),
            [hostAddress](const AddressEntry& entry)
            {
                return entry.type == AddressType::direct && entry.host == hostAddress;
            });
        ASSERT_TRUE(entryIter != entries.end());
    }

    void assertDnsResolverHasNotBeenInvoked()
    {
        ASSERT_TRUE(m_hostnamesPassedToDnsResolver.empty());
    }

    void assertResolveIsDoneByDnsResolver()
    {
        thenResolveSucceeded();

        ASSERT_EQ(1U, std::get<1>(m_prevResolveResult).size());
        ASSERT_EQ(AddressType::direct, std::get<1>(m_prevResolveResult).front().type);
        ASSERT_EQ(*m_dnsEntry, std::get<1>(m_prevResolveResult).front().host);
    }

    network::AddressResolver& resolver()
    {
        return m_resolver;
    }

private:
    using ResolveResult =
        std::tuple<SystemError::ErrorCode, std::deque<AddressEntry>>;

    network::AddressResolver m_resolver;
    int m_ipVersionToUse = AF_INET;
    QString m_hostNameToResolve;
    boost::optional<HostAddress> m_dnsEntry;
    std::list<QString> m_hostnamesPassedToDnsResolver;
    nx::utils::SyncQueue<ResolveResult> m_resolveResults;
    ResolveResult m_prevResolveResult;

    SystemError::ErrorCode saveHostNameWithoutResolving(
        const QString& hostName,
        int /*ipVersion*/,
        std::deque<AddressEntry>* /*resolvedAddresses*/)
    {
        m_hostnamesPassedToDnsResolver.push_back(hostName);
        return SystemError::hostNotFound;
    }
};

TEST_F(
    AddressResolver,
    ipv4_resolve_does_not_invoke_dns_for_text_representation_of_ip_address)
{
    givenStringRepresentationOfIpv4AddressAsHostname();
    whenResolve();
    assertDnsResolverHasNotBeenInvoked();
}

TEST_F(
    AddressResolver,
    ipv6_resolve_invokes_dns_for_text_representation_of_ip_address)
{
    setIpVersionToUse(AF_INET6);

    givenStringRepresentationOfIpv4AddressAsHostname();
    emulateHostnameResolvableByDnsResolver();

    whenResolve();

    assertResolveIsDoneByDnsResolver();
}

TEST_F(AddressResolver, localhost_is_resolved_properly_ipv4)
{
    whenResolve("localhost");

    thenResolveSucceeded();
    andResolvedTo(HostAddress(in4addr_loopback));
}

TEST_F(AddressResolver, localhost_is_resolved_properly_ipv6)
{
    setIpVersionToUse(AF_INET6);

    whenResolve("localhost");

    thenResolveSucceeded();
    andResolvedTo(HostAddress(in4addr_loopback));
    andResolvedTo(HostAddress(in6addr_loopback, 0));
}

TEST_F(AddressResolver, resolving_same_name_with_different_ip_version)
{
    whenResolve("localhost");
    thenResolveSucceeded();

    setIpVersionToUse(AF_INET6);

    whenResolve("localhost");

    thenResolveSucceeded();
    andResolvedTo(HostAddress(in4addr_loopback));
    andResolvedTo(HostAddress(in6addr_loopback, 0));
}

//-------------------------------------------------------------------------------------------------

class AddressResolverNameMapping:
    public AddressResolver
{
public:
    AddressResolverNameMapping()
    {
        // Disabling DNS resolve.
        using namespace std::placeholders;

        struct in_addr stubAddress;
        memset(&stubAddress, 0, sizeof(stubAddress));
        stubAddress.s_addr = nx::utils::random::number<std::uint32_t>();
        m_stubAddress = HostAddress(stubAddress);

        resolver().dnsResolver().registerResolver(
            makeCustomResolver(std::bind(
                &AddressResolverNameMapping::dnsResolveStub, this, _1, _2, _3)),
            resolver().dnsResolver().maxRegisteredResolverPriority() + 1);
    }

protected:
    void registerFixedHighLevelDomainName()
    {
        m_hostNameToResolve = "a.b";
        m_hostEndpoint = SocketAddress(
            HostAddress::localhost,
            nx::utils::random::number<std::uint16_t>(1));
        resolver().addFixedAddress(m_hostNameToResolve, m_hostEndpoint);
    }

    void unregisterDomainName()
    {
        resolver().removeFixedAddress(m_hostNameToResolve, m_hostEndpoint);
    }

    void registerMultipleEndpointsUnderSingleName()
    {
        m_hostNameToResolve = "example.com";
        for (unsigned int i = 0; i < 3; ++i)
        {
            struct in_addr inAddr;
            memset(&inAddr, 0, sizeof(inAddr));
            inAddr.s_addr = 1000 + i;
            HostAddress address(inAddr);
            const SocketAddress endpoint(address, 12345);
            m_fixedEntries.push_back(endpoint);
            resolver().addFixedAddress(m_hostNameToResolve, endpoint);
        }
    }

    void removeRandomFixedEndpoint()
    {
        const auto endpointIndexToRemove =
            nx::utils::random::number<std::vector<SocketAddress>::size_type>(
                0, m_fixedEntries.size() - 1);
        resolver().removeFixedAddress(
            m_hostNameToResolve,
            m_fixedEntries[endpointIndexToRemove]);
        m_fixedEntries.erase(m_fixedEntries.begin() + endpointIndexToRemove);
    }

    void whenResolveDomain(const QString& domainName)
    {
        using namespace std::placeholders;

        resolver().resolveAsync(
            domainName,
            std::bind(&AddressResolverNameMapping::saveResolveResult, this, _1, _2),
            NatTraversalSupport::disabled,
            AF_INET);
    }

    void assertLowLevelDomainNameCanBeResolved()
    {
        whenResolveDomain(m_hostNameToResolve.split(".").last());
        thenResolvedTo(m_hostEndpoint);
    }

    void assertLowLevelDomainNameCannotBeResolved()
    {
        whenResolveDomain(m_hostNameToResolve.split(".").last());
        thenResolvedResultIs(SystemError::hostNotFound);
    }

    void assertFixedEndpointsCanStillBeResolved()
    {
        const auto entries = resolver().resolveSync(
            m_hostNameToResolve, NatTraversalSupport::disabled, AF_INET);
        ASSERT_EQ(entries.size(), m_fixedEntries.size());
        for (decltype(entries)::size_type i = 0; i < entries.size(); ++i)
        {
            ASSERT_EQ(entries[i].toEndpoint(), m_fixedEntries[i]);
        }
    }

    void thenResolvedTo(const SocketAddress& hostEndpoint)
    {
        const auto resolveResult = m_resolveResults.pop();
        ASSERT_EQ(SystemError::noError, std::get<0>(resolveResult));
        const auto& entries = std::get<1>(resolveResult);
        ASSERT_EQ(1U, entries.size());
        ASSERT_EQ(AddressType::direct, entries[0].type);
        ASSERT_EQ(hostEndpoint.address, entries[0].host);
    }

    void thenResolvedResultIs(SystemError::ErrorCode systemErrorCode)
    {
        const auto resolveResult = m_resolveResults.pop();
        ASSERT_EQ(systemErrorCode, std::get<0>(resolveResult));
    }

private:
    using ResolveResult =
        std::tuple<SystemError::ErrorCode, std::deque<AddressEntry>>;

    QString m_hostNameToResolve;
    SocketAddress m_hostEndpoint;
    nx::utils::SyncQueue<ResolveResult> m_resolveResults;
    HostAddress m_stubAddress;
    std::vector<SocketAddress> m_fixedEntries;

    SystemError::ErrorCode dnsResolveStub(
        const QString& /*hostName*/,
        int /*ipVersion*/,
        std::deque<AddressEntry>* resolvedAddresses)
    {
        resolvedAddresses->push_back({AddressType::direct, m_stubAddress});
        return SystemError::noError;
    }

    void saveResolveResult(
        SystemError::ErrorCode errorCode,
        std::deque<AddressEntry> entries)
    {
        if (errorCode == SystemError::noError && entries.front().host == m_stubAddress)
        {
            errorCode = SystemError::hostNotFound;
            entries.clear();
        }
        m_resolveResults.push(std::make_tuple(errorCode, std::move(entries)));
    }
};

TEST_F(
    AddressResolverNameMapping,
    low_level_domain_is_resolved_to_address_of_a_high_level_domain)
{
    registerFixedHighLevelDomainName();
    assertLowLevelDomainNameCanBeResolved();
}

TEST_F(
    AddressResolverNameMapping,
    after_removing_hostname_mapping_low_level_domain_cannot_be_resolved)
{
    registerFixedHighLevelDomainName();
    unregisterDomainName();

    assertLowLevelDomainNameCannotBeResolved();
}

TEST_F(
    AddressResolverNameMapping,
    mapping_multiple_addresses_under_a_single_hostname)
{
    registerMultipleEndpointsUnderSingleName();
    removeRandomFixedEndpoint();
    assertFixedEndpointsCanStillBeResolved();
}

//-------------------------------------------------------------------------------------------------

class AddressResolverNat64:
    public ::testing::Test
{
public:
    AddressResolverNat64()
    {
        nx::network::SocketGlobalsHolder::instance()->reinitialize();
        std::vector<HostAddress> ipList{HostAddress(*kV6.ipV6().first), HostAddress(*kV4.ipV4())};
        SocketGlobals::addressResolver().dnsResolver().addEtcHost(kV4.toString(), ipList);
    }

    ~AddressResolverNat64()
    {
        SocketGlobals::addressResolver().dnsResolver().removeEtcHost("192.168.1.2");
    }

    static const HostAddress kV4;
    static const HostAddress kV6;
};

const HostAddress AddressResolverNat64::kV4("192.168.1.2");
const HostAddress AddressResolverNat64::kV6("2001:db8:0:2::1");

TEST_F(AddressResolverNat64, IPv4) // NAT64 not in use, IP v4 is just converted for better speed.
{
    const auto entries = SocketGlobals::addressResolver().resolveSync(
        kV4, NatTraversalSupport::disabled, AF_INET);
    ASSERT_EQ(1U, entries.size());

    ASSERT_EQ(AddressType::direct, entries.front().type);
    ASSERT_TRUE(entries.front().host.isIpAddress());
    ASSERT_EQ(kV4.ipV4()->s_addr, entries.front().host.ipV4()->s_addr);
}

TEST_F(AddressResolverNat64, IPv6) // NAT64 returns 2 addresses: mapped IP v6 and converted IP v4.
{
    const auto entries = SocketGlobals::addressResolver().resolveSync(
        kV4, NatTraversalSupport::disabled, AF_INET6);
    ASSERT_EQ(2U, entries.size());

    ASSERT_EQ(AddressType::direct, entries.front().type);
    ASSERT_TRUE(entries.front().host.isIpAddress());
    ASSERT_EQ(0, memcmp(&kV6.ipV6().first.get(), &entries.front().host.ipV6().first.get(), sizeof(in6_addr)));

    ASSERT_EQ(AddressType::direct, entries.back().type);
    ASSERT_TRUE(entries.back().host.isIpAddress());
    ASSERT_EQ(0, memcmp(&kV4.ipV6().first.get(), &entries.back().host.ipV6().first.get(), sizeof(in6_addr)));
}

} // namespace test
} // namespace network
} // namespace nx
