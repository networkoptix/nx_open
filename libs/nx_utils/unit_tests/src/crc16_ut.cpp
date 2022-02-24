// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <chrono>
#include <cstdlib>

#include <gtest/gtest.h>

#include <nx/utils/crc16.h>

namespace nx::utils {

TEST(Crc16, invalidArgsHandledCorrectly)
{
    const char test[] = "";
    const auto result = nx::utils::crc16(test, sizeof(test) - 1);
    ASSERT_EQ(result, 0x0);
}

TEST(Crc16, resultIsCorrect)
{
    {
        const char test[] = "123456789";
        const auto result = nx::utils::crc16(test, sizeof(test) - 1);
        ASSERT_EQ(result, 0xBB3D);
    }
    {
        const char test[] = "asdfbasjkfbaslbkclbhlasblhdcbjlasbh";
        const auto result = nx::utils::crc16(test, sizeof(test) - 1);
        ASSERT_EQ(result, 0xC322);
    }
}

} // namespace nx::utils
