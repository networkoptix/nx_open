// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <common/common_globals.h>
#include <nx/vms/api/data/os_information.h>

using nx::vms::api::OsInformation;

TEST(OsInformation, ToString)
{
    {
        const OsInformation info;
        EXPECT_FALSE(info.isValid());
        EXPECT_EQ(info.toString(), " ");
    }
    {
        const OsInformation info("hello", "world");
        EXPECT_TRUE(info.isValid());
        EXPECT_EQ(info.toString(), "hello world");
    }
    {
        const OsInformation info("hello", "world", "!");
        EXPECT_TRUE(info.isValid());
        EXPECT_EQ(info.toString(), "hello world !");
    }
    {
        const auto info = OsInformation::fromBuildInfo();
        EXPECT_TRUE(info.isValid());
        EXPECT_GT(info.toString().length(), 0);
    }
}

TEST(OsInformation, FromString)
{
    {
        const OsInformation info("linux rules");
        EXPECT_TRUE(info.isValid());

        EXPECT_EQ(info.platform, "linux");
        EXPECT_EQ(info.arch, "rules");
        EXPECT_EQ(info.modification, "");
        EXPECT_EQ(info.toString(), "linux rules");
    }
    {
        const OsInformation info("windows sucks :)");
        EXPECT_TRUE(info.isValid());

        EXPECT_EQ(info.platform, "windows");
        EXPECT_EQ(info.arch, "sucks");
        EXPECT_EQ(info.modification, ":)");
        EXPECT_EQ(info.toString(), "windows sucks :)");
    }
    {
        const auto curInfo = OsInformation::fromBuildInfo();
        const OsInformation parsedInfo(curInfo.toString());

        EXPECT_GT(parsedInfo.arch.length(), 0);
        EXPECT_GT(parsedInfo.platform.length(), 0);

        EXPECT_EQ(parsedInfo.arch, curInfo.arch);
        EXPECT_EQ(parsedInfo.platform, curInfo.platform);
        EXPECT_EQ(parsedInfo.modification, curInfo.modification);
    }
}
