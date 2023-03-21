// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <health/system_health_strings_helper.h>

TEST(systemHealthStringsHelper, checkMessageTitles)
{
    for (auto messageType: nx::vms::common::system_health::allVisibleMessageTypes())
    {
        EXPECT_FALSE(QnSystemHealthStringsHelper::messageTitle(messageType).isEmpty());
    }
}
