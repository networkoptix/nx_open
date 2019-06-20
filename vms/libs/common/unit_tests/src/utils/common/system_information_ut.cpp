#include <gtest/gtest.h>

#include <common/common_globals.h>
#include <utils/common/app_info.h>

using nx::vms::api::SystemInformation;

TEST(SystemInformation, ToString)
{
    {
        const SystemInformation info;
        EXPECT_FALSE(info.isValid());
        EXPECT_EQ(info.toString(), " ");
    }
    {
        const SystemInformation info("hello", "world");
        EXPECT_TRUE(info.isValid());
        EXPECT_EQ(info.toString(), "hello world");
    }
    {
        const SystemInformation info("hello", "world", "!");
        EXPECT_TRUE(info.isValid());
        EXPECT_EQ(info.toString(), "hello world !");
    }
    {
        const auto info = QnAppInfo::currentSystemInformation();
        EXPECT_TRUE(info.isValid());
        EXPECT_GT(info.toString().length(), 0);
    }
}

TEST(SystemInformation, FromString)
{
    {
        const SystemInformation info("linux rules");
        EXPECT_TRUE(info.isValid());

        EXPECT_EQ(info.platform, "linux");
        EXPECT_EQ(info.arch, "rules");
        EXPECT_EQ(info.modification, "");
        EXPECT_EQ(info.toString(), "linux rules");
    }
    {
        const SystemInformation info("windows sucks :)");
        EXPECT_TRUE(info.isValid());

        EXPECT_EQ(info.platform, "windows");
        EXPECT_EQ(info.arch, "sucks");
        EXPECT_EQ(info.modification, ":)");
        EXPECT_EQ(info.toString(), "windows sucks :)");
    }
    {
        const auto curInfo = QnAppInfo::currentSystemInformation();
        const SystemInformation parsedInfo(curInfo.toString());

        EXPECT_GT(parsedInfo.arch.length(), 0);
        EXPECT_GT(parsedInfo.platform.length(), 0);

        EXPECT_EQ(parsedInfo.arch, curInfo.arch);
        EXPECT_EQ(parsedInfo.platform, curInfo.platform);
        EXPECT_EQ(parsedInfo.modification, curInfo.modification);
    }
}
