#include <gtest/gtest.h>

#include <nx/network/resolve/predefined_host_resolver.h>
#include <nx/utils/random.h>

namespace nx {
namespace network {
namespace test {

class PredefinedHostResolver:
    public ::testing::Test
{
protected:
    void mapMultipleEntriesUnderSingleName()
    {
        m_hostname = "example.com";
        for (int i = 0; i < 3; ++i)
        {
            AddressEntry entry(SocketAddress(
                HostAddress::localhost, nx::utils::random::number<int>(10000, 20000)));
            m_mappedEntries.push_back(entry);
        }
        m_predefinedHostResolver.addMapping(m_hostname, m_mappedEntries);
    }

    void unmapRandomEntry()
    {
        const auto indexToRemove = nx::utils::random::number<std::size_t>(
            0, m_mappedEntries.size() - 1);
        m_predefinedHostResolver.removeMapping(m_hostname, m_mappedEntries[indexToRemove]);
        m_mappedEntries.erase(m_mappedEntries.begin() + indexToRemove);
    }

    void unmapAllEntries()
    {
        m_predefinedHostResolver.removeMapping(m_hostname);
    }

    void assertHostnameIsNotResolved()
    {
        std::deque<AddressEntry> resolvedEntries;
        ASSERT_EQ(
            SystemError::hostNotFound,
            m_predefinedHostResolver.resolve(m_hostname, AF_INET, &resolvedEntries));
    }

    void assertAllMappedEntriesAreResolved()
    {
        std::deque<AddressEntry> resolvedEntries;
        ASSERT_EQ(
            SystemError::noError,
            m_predefinedHostResolver.resolve(m_hostname, AF_INET, &resolvedEntries));
        ASSERT_EQ(m_mappedEntries, resolvedEntries);
    }

private:
    network::PredefinedHostResolver m_predefinedHostResolver;
    QString m_hostname;
    std::deque<AddressEntry> m_mappedEntries;
};

TEST_F(PredefinedHostResolver, mapping_multiple_addresses_under_a_single_hostname)
{
    mapMultipleEntriesUnderSingleName();
    assertAllMappedEntriesAreResolved();
}

TEST_F(PredefinedHostResolver, unmapping_one_of_many_mapped_endpoints_of_a_hostname)
{
    mapMultipleEntriesUnderSingleName();
    unmapRandomEntry();
    assertAllMappedEntriesAreResolved();
}

TEST_F(PredefinedHostResolver, host_is_not_resolved_after_removing_all_mappings)
{
    mapMultipleEntriesUnderSingleName();
    unmapAllEntries();
    assertHostnameIsNotResolved();
}

} // namespace test
} // namespace network
} // namespace nx
