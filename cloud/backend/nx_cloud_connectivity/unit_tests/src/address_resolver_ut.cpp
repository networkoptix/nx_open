#include <thread>

#include <gtest/gtest.h>

#include <nx/network/address_resolver.h>
#include <nx/utils/test_support/sync_queue.h>
#include <nx/utils/uuid.h>

namespace nx {
namespace network {
namespace cloud {
namespace test {

/**
 * For every address expecting following resolve order:
 * - Fixed address table.
 * - cloud resolve, if and only if hostname looks like cloud address ("guid" or "guid.guid").
 * - Regular DNS.
 */
class AddressResolverOrder:
    public ::testing::Test
{
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

    void disabledCloudResolve()
    {
        m_cloudResolveConfiguration = NatTraversalSupport::disabled;
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
    nx::network::AddressResolver m_addressResolver;
    QString m_hostname;
    SocketAddress m_fixedEntry;
    HostAddress m_dnsEntry;
    SystemError::ErrorCode m_resolveResult = SystemError::noError;
    std::deque<AddressEntry> m_resolvedEntries;
    NatTraversalSupport m_cloudResolveConfiguration = NatTraversalSupport::enabled;

    void resolve()
    {
        m_resolvedEntries = m_addressResolver.resolveSync(
            m_hostname, m_cloudResolveConfiguration, AF_INET);
        if (m_resolvedEntries.empty())
            m_resolveResult = SystemError::getLastOSErrorCode();
    }
};

//-------------------------------------------------------------------------------------------------

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
    cloud_resolve_is_skipped_if_not_requested)
{
    disabledCloudResolve();

    givenCloudHostname();
    makeHostnameResolvableWithDns();
    assertResolveReturnsDnsResolveResult();
}

} // namespace test
} // namespace cloud
} // namespace network
} // namespace nx
