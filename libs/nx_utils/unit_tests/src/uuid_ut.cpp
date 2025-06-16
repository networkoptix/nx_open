// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/utils/uuid.h>

TEST(Uuid, valid_uuid_string)
{
    ASSERT_TRUE(nx::Uuid::isValidUuidString(std::string("fc049201-c4ec-4ea7-81da-2afcc8fbfd3a")));
}

TEST(Uuid, invalid_uuid_string)
{
    ASSERT_FALSE(nx::Uuid::isValidUuidString(std::string("fc049201-c4ec-4ea7-81da-2afcc8fbfd3a@nxtest.com")));
}

TEST(Uuid, uuidV7WithCustomTime)
{
    using namespace std::chrono;
    const auto expectedTime =
        duration_cast<milliseconds>(system_clock().now().time_since_epoch());
    const auto uuid = nx::Uuid::createUuidV7(expectedTime);
    ASSERT_TRUE(uuid.timeSinceEpoch());
    ASSERT_EQ(uuid.timeSinceEpoch(), expectedTime);
    const auto data = uuid.toRfc4122();
    const auto shift = [](uint8_t data, int bits) { return uint64_t(data) << bits; };
    uint64_t rawTimeMs = 0;
    for (int i = 0; i < 6; ++i)
        rawTimeMs |= shift(data[i] , (40 - i * 8));
    EXPECT_EQ(rawTimeMs, (uint64_t) expectedTime.count());
    const QUuid qtUuid = uuid.getQUuid();
    EXPECT_EQ(qtUuid.variant(), QUuid::Variant::DCE);
    EXPECT_EQ(qtUuid.version(), QUuid::Version::UnixEpoch);
    // Check that it is used properly with all those endians.
    const auto copyUuid = nx::Uuid::fromRfc4122(uuid.toRfc4122());
    ASSERT_EQ(copyUuid, uuid);
    ASSERT_TRUE(copyUuid.timeSinceEpoch());
    ASSERT_EQ(copyUuid.timeSinceEpoch(), expectedTime);
}
