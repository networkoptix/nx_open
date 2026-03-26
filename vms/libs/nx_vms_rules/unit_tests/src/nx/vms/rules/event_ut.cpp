// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <QtNetwork/QHostAddress>

#include <nx/vms/rules/events/device_ip_conflict_event.h>

namespace nx::vms::rules::test {

TEST(DeviceIpConflict, macAddressesSyncWithFilteredDeviceIds)
{
    const auto deviceId1 = nx::Uuid::createUuid();
    const auto deviceId2 = nx::Uuid::createUuid();
    const auto mac1 = "aa:bb:cc:dd:ee:01";
    const auto mac2 = "aa:bb:cc:dd:ee:02";

    auto event = QSharedPointer<DeviceIpConflictEvent>::create();
    event->setDeviceIds({deviceId1, deviceId2});
    event->setMacAddresses({mac1, mac2});

    ASSERT_EQ(event->deviceIds().size(), 2);
    ASSERT_EQ(event->macAddresses().size(), 2);

    event = QSharedPointer<DeviceIpConflictEvent>::create();
    event->setMacAddresses({mac1, mac2});
    event->setDeviceIds({deviceId1, deviceId2});

    ASSERT_EQ(event->deviceIds().size(), 2);
    ASSERT_EQ(event->macAddresses().size(), 2);

    // Simulate permission filtering: keep only deviceId2.
    event->setDeviceIds({deviceId2});

    EXPECT_EQ(event->deviceIds().size(), 1);
    EXPECT_EQ(event->macAddresses().size(), 1);
    EXPECT_EQ(event->deviceIds().first(), deviceId2);
    EXPECT_EQ(event->macAddresses().first(), mac2);
}

} // namespace nx::vms::rules::test
