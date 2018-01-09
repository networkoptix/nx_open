#include <memory>

#include <gtest/gtest.h>

#include <nx/network/aio/aio_service.h>
#include <nx/network/abstract_socket.h>
#include <nx/network/address_resolver.h>
#include <nx/network/dns_resolver.h>
#include <nx/network/resolve/custom_resolver.h>
#include <nx/network/socket_global.h>
#include <nx/network/socket_factory.h>
#include <nx/network/system_socket.h>
#include <nx/utils/random.h>
#include <nx/utils/std/future.h>

namespace nx {
namespace network {
namespace test {

namespace {

class AddressResolver:
    public testing::Test
{
public:
    AddressResolver():
        m_startedConnectionsCount(0),
        m_completedConnectionsCount(0)
    {
    }

protected:
    std::mutex m_mutex;
    std::atomic<int> m_startedConnectionsCount;
    std::atomic<int> m_completedConnectionsCount;
    std::deque<std::unique_ptr<AbstractStreamSocket>> m_connections;

    void startAnotherSocketNonSafe()
    {
        std::unique_ptr<AbstractStreamSocket> connection(SocketFactory::createStreamSocket());
        ASSERT_TRUE(connection->setNonBlockingMode(true));
        AbstractStreamSocket* connectionPtr = connection.get();
        m_connections.push_back(std::move(connection));
        connectionPtr->connectAsync(
            SocketAddress(QString::fromLatin1("ya.ru"), nx::network::http::DEFAULT_HTTP_PORT),
            std::bind(&AddressResolver::onConnectionComplete, this, connectionPtr, std::placeholders::_1));
    }

    void onConnectionComplete(
        AbstractStreamSocket* connectionPtr,
        SystemError::ErrorCode errorCode)
    {
        ASSERT_TRUE(errorCode == SystemError::noError);

        std::unique_lock<std::mutex> lk(m_mutex);

        auto iterToRemove = std::remove_if(
            m_connections.begin(), m_connections.end(),
            [connectionPtr](const std::unique_ptr<AbstractStreamSocket>& elem) -> bool
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

// TODO: #ak Most of these tests do not belong here.
// They actually check how socket use AddressResolver, not how AddressResolver works.
// So, these tests should belong to some stream socket acceptance test group.

TEST_F(AddressResolver, HostNameResolve2)
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

class AddressResolverTrivialNameResolve:
    public ::testing::Test
{
public:
    AddressResolverTrivialNameResolve():
        m_hostNameToResolve("127.0.0.1")
    {
        using namespace std::placeholders;

        m_resolver.dnsResolver().registerResolver(
            makeCustomResolver(std::bind(
                &AddressResolverTrivialNameResolve::saveHostNameWithoutResolving, this, _1, _2, _3)),
            m_resolver.dnsResolver().maxRegisteredResolverPriority() + 1);
    }

protected:
    void whenResolvingStringRepresentationOfIpAddress()
    {
        m_resolver.resolveSync(m_hostNameToResolve, NatTraversalSupport::disabled, AF_INET);
    }

    void assertIfDnsResolverHasBeenInvoked()
    {
        ASSERT_EQ(
            m_resolvedHostNames.end(),
            std::find(m_resolvedHostNames.begin(), m_resolvedHostNames.end(), m_hostNameToResolve));
    }

private:
    network::AddressResolver m_resolver;
    const QString m_hostNameToResolve;
    std::list<QString> m_resolvedHostNames;

    SystemError::ErrorCode saveHostNameWithoutResolving(
        const QString& hostName,
         int /*ipVersion*/,
         std::deque<HostAddress>* /*resolvedAddresses*/)
    {
        m_resolvedHostNames.push_back(hostName);
        return SystemError::hostNotFound;
    }
};

TEST_F(AddressResolverTrivialNameResolve, DoesNotInvokeDns)
{
    whenResolvingStringRepresentationOfIpAddress();
    assertIfDnsResolverHasBeenInvoked();
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
