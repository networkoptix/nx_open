#include <gtest/gtest.h>

#include <common/common_globals.h>
#include <utils/common/app_info.h>

using nx::vms::api::SystemInformation;

TEST( SystemInformation, ToString )
{
    {
        const SystemInformation info;
        EXPECT_FALSE( info.isValid() );
        EXPECT_EQ( info.toString(), lit(" ") );
    }
    {
        const SystemInformation info( lit("hello"), lit("world") );
        EXPECT_TRUE( info.isValid() );
        EXPECT_EQ( info.toString(), lit("hello world") );
    }
    {
        const SystemInformation info( lit("hello"), lit("world"), lit("!") );
        EXPECT_TRUE( info.isValid() );
        EXPECT_EQ( info.toString(), lit("hello world !") );
    }
    {
        const auto info = QnAppInfo::currentSystemInformation();
        EXPECT_TRUE( info.isValid() );
        EXPECT_GT( info.toString().length(), 0 );
    }
}

TEST( SystemInformation, FromString )
{
    {
        const SystemInformation info( lit("linux rules") );
        EXPECT_TRUE( info.isValid() );

        EXPECT_EQ( info.platform,       lit("linux") );
        EXPECT_EQ( info.arch,           lit("rules") );
        EXPECT_EQ( info.modification,   lit("") );
        EXPECT_EQ( info.toString(),     lit("linux rules") );
    }
    {
        const SystemInformation info( lit("windows sucks :)") );
        EXPECT_TRUE( info.isValid() );

        EXPECT_EQ( info.platform,       lit("windows") );
        EXPECT_EQ( info.arch,           lit("sucks") );
        EXPECT_EQ( info.modification,   lit(":)") );
        EXPECT_EQ( info.toString(),     lit("windows sucks :)") );
    }
    {
        const auto curInfo = QnAppInfo::currentSystemInformation();
        const SystemInformation parsedInfo( curInfo.toString() );

        EXPECT_GT( parsedInfo.arch.length(),            0 );
        EXPECT_GT( parsedInfo.platform.length(),        0 );
        EXPECT_GT( parsedInfo.modification.length(),    0 );

        EXPECT_EQ( parsedInfo.arch,         curInfo.arch );
        EXPECT_EQ( parsedInfo.platform,     curInfo.platform );
        EXPECT_EQ( parsedInfo.modification, curInfo.modification );
    }
}
