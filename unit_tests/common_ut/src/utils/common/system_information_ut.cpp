#include <gtest/gtest.h>

#include <common/common_globals.h>
#include <utils/common/system_information.h>

TEST( QnSystemInformation, ToString )
{
    {
        const QnSystemInformation info;
        EXPECT_FALSE( info.isValid() );
        EXPECT_EQ( info.toString(), lit(" ") );
    }
    {
        const QnSystemInformation info( lit("hello"), lit("world") );
        EXPECT_TRUE( info.isValid() );
        EXPECT_EQ( info.toString(), lit("hello world") );
    }
    {
        const QnSystemInformation info( lit("hello"), lit("world"), lit("!") );
        EXPECT_TRUE( info.isValid() );
        EXPECT_EQ( info.toString(), lit("hello world !") );
    }
    {
        const auto info = QnSystemInformation::currentSystemInformation();
        EXPECT_TRUE( info.isValid() );
        EXPECT_GT( info.toString().length(), 0 );
    }
}

TEST( QnSystemInformation, FromString )
{
    {
        const QnSystemInformation info( lit("linux rules") );
        EXPECT_TRUE( info.isValid() );

        EXPECT_EQ( info.platform,       lit("linux") );
        EXPECT_EQ( info.arch,           lit("rules") );
        EXPECT_EQ( info.modification,   lit("") );
        EXPECT_EQ( info.toString(),     lit("linux rules") );
    }
    {
        const QnSystemInformation info( lit("windows sucks :)") );
        EXPECT_TRUE( info.isValid() );

        EXPECT_EQ( info.platform,       lit("windows") );
        EXPECT_EQ( info.arch,           lit("sucks") );
        EXPECT_EQ( info.modification,   lit(":)") );
        EXPECT_EQ( info.toString(),     lit("windows sucks :)") );
    }
    {
        const auto curInfo = QnSystemInformation::currentSystemInformation();
        const QnSystemInformation parsedInfo( curInfo.toString() );

        EXPECT_GT( parsedInfo.arch.length(),            0 );
        EXPECT_GT( parsedInfo.platform.length(),        0 );
        EXPECT_GT( parsedInfo.modification.length(),    0 );

        EXPECT_EQ( parsedInfo.arch,         curInfo.arch );
        EXPECT_EQ( parsedInfo.platform,     curInfo.platform );
        EXPECT_EQ( parsedInfo.modification, curInfo.modification );
    }
}
