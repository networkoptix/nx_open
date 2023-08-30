// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/vms/common/resource/server_host_priority.h>

namespace nx::vms::common::test {

static const QString cloud("94ca45f8-4859-457a-bc17-f2f9394524fe");
static const QString localHost("127.0.0.1");
static const QString localIpV6("fd00::9465:d2ff:fe64:2772");
static const QString remoteIp("1.2.3.4");
static const QString localIpV4("10.0.3.14");
static const QString dns("test.com");

TEST(ServerHostPriority, mainServerUrl)
{
    using namespace nx::vms::common;
    QSet<QString> remoteAddresses;

    remoteAddresses.insert(cloud);
    ASSERT_EQ(mainServerUrl(remoteAddresses).host(), cloud);

    remoteAddresses.insert(localHost);
    ASSERT_EQ(mainServerUrl(remoteAddresses).host(), localHost);

    remoteAddresses.insert(localIpV6);
    ASSERT_EQ(mainServerUrl(remoteAddresses).host(), localIpV6);

    remoteAddresses.insert(remoteIp);
    ASSERT_EQ(mainServerUrl(remoteAddresses).host(), remoteIp);

    remoteAddresses.insert(localIpV4);
    ASSERT_EQ(mainServerUrl(remoteAddresses).host(), localIpV4);

    remoteAddresses.insert(dns);
    ASSERT_EQ(mainServerUrl(remoteAddresses).host(), dns);
}

} // namespace nx::vms::common::test
