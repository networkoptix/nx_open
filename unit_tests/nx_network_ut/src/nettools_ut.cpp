#include <gtest/gtest.h>

#include <nx/network/nettools.h>

namespace test {

TEST(getAllLocalAddresses, returns_valid_ip_addresses)
{
    const auto localAddresses = allLocalAddresses(AddressFilter::ipV4 | AddressFilter::ipV6);
    for (const auto& addr: localAddresses)
    {
        ASSERT_TRUE(addr.ipV4() || addr.ipV6()) << addr.toString().toStdString();
    }
}

} // namespace test
