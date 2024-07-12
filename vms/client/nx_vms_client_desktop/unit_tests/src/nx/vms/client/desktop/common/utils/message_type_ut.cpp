// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <health/system_health_strings_helper.h>

TEST(systemHealthStringsHelper, checkMessageTitles)
{
    using namespace nx::vms::common;
    for (auto messageType:
        system_health::allMessageTypes({system_health::isMessageVisibleInSettings}))
    {
        EXPECT_FALSE(QnSystemHealthStringsHelper::messageShortTitle(messageType).isEmpty());
    }
}
