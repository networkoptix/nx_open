// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

// This header includes <windows.h> and _must_ be included before everything
#include <nx/utils/system_network_headers.h>
/////////////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>

#include <nx/network/nettools.h>

namespace nx {
namespace network {
namespace test {

TEST(getAllLocalAddresses, returns_valid_ip_addresses)
{
    const auto localAddresses = allLocalAddresses(AddressFilter::ipV4 | AddressFilter::ipV6);
    for (const auto& addr: localAddresses)
    {
        ASSERT_TRUE(addr.ipV4() || addr.ipV6().first) << addr.toString();
    }
}

} // namespace test
} // namespace network
} // namespace nx
