// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/network/resolve/predefined_host_resolver.h>
#include <nx/utils/random.h>
#include <nx/utils/std/algorithm.h>

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
        auto entry = generateRandomEntry();
        for (int i = 0; i < 3; ++i)
        {
            ++entry.attributes[0].value;
            m_mappedEntries.push_back(entry);
        }

        m_predefinedHostResolver.replaceMapping(m_hostname, m_mappedEntries);
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

    AddressEntry mapNameToRandomEntry(const std::string& hostname)
    {
        const auto entry = generateRandomEntry();
        m_predefinedHostResolver.addMapping(hostname, {entry});
        return entry;
    }

    void assertNameIsResolved(const std::string& hostname)
    {
        ASSERT_EQ(
            SystemError::noError,
            m_predefinedHostResolver.resolve(hostname, AF_INET, &m_lastResolveResult));
        ASSERT_FALSE(m_lastResolveResult.entries.empty());
    }

    void assertNameIsNotResolved(const std::string& hostname)
    {
        ASSERT_NE(
            SystemError::noError,
            m_predefinedHostResolver.resolve(hostname, AF_INET, &m_lastResolveResult));
    }

    void assertHostnameIsNotResolved()
    {
        ResolveResult resolved;
        ASSERT_EQ(
            SystemError::hostNotFound,
            m_predefinedHostResolver.resolve(m_hostname, AF_INET, &resolved));
    }

    void assertAllMappedEntriesAreResolved()
    {
        ResolveResult resolved;
        ASSERT_EQ(
            SystemError::noError,
            m_predefinedHostResolver.resolve(m_hostname, AF_INET, &resolved));
        ASSERT_EQ(m_mappedEntries, resolved.entries);
    }

    void assertNameIsResolvedTo(
        const std::string& hostname,
        const AddressEntry& expectedEntry)
    {
        assertNameIsResolved(hostname);
        ASSERT_TRUE(nx::utils::contains(m_lastResolveResult.entries, expectedEntry));
    }

private:
    network::PredefinedHostResolver m_predefinedHostResolver;
    std::string m_hostname;
    std::deque<AddressEntry> m_mappedEntries;
    ResolveResult m_lastResolveResult;
    int m_sequence = -1;

    AddressEntry generateRandomEntry()
    {
        if (m_sequence < 0)
            m_sequence = nx::utils::random::number<int>(10000, 20000);

        return AddressEntry(SocketAddress(HostAddress::localhost, m_sequence++));
    }
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

TEST_F(PredefinedHostResolver, low_level_domain_is_resolved_to_a_higher_level_name)
{
    mapNameToRandomEntry("a.b");
    assertNameIsResolved("b");
    assertNameIsNotResolved("a");
}

TEST_F(PredefinedHostResolver, low_level_domain_is_resolved_if_mapped)
{
    const auto abEntry = mapNameToRandomEntry("a.b");
    const auto aEntry = mapNameToRandomEntry("b");

    assertNameIsResolvedTo("a.b", abEntry);
    assertNameIsResolvedTo("b", aEntry);
}

} // namespace test
} // namespace network
} // namespace nx
