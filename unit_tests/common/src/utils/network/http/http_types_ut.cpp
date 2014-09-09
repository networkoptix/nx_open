/**********************************************************
* 8 sep 2014
* a.kolesnikov
***********************************************************/

#include <gtest/gtest.h>

#include <utils/network/http/httptypes.h>


TEST( HttpRangeHeaderTest, parse )
{
    nx_http::header::Range range;
    range.parse( "650-" );
    ASSERT_EQ( range.rangeSpecList.size(), 1 );
    ASSERT_EQ( range.rangeSpecList[0].start, 650 );
    ASSERT_FALSE( range.rangeSpecList[0].end );

    range.rangeSpecList.clear();
    range.parse( "100,200-230" );
    ASSERT_EQ( range.rangeSpecList.size(), 2 );
    ASSERT_EQ( range.rangeSpecList[0].start, 100 );
    ASSERT_TRUE( range.rangeSpecList[0].end );
    ASSERT_EQ( range.rangeSpecList[0].end.get(), 100 );
    ASSERT_EQ( range.rangeSpecList[1].start, 200 );
    ASSERT_EQ( range.rangeSpecList[1].end.get(), 230 );

    range.rangeSpecList.clear();
    range.parse( "200-230" );
    ASSERT_EQ( range.rangeSpecList.size(), 1 );
    ASSERT_EQ( range.rangeSpecList[0].start, 200 );
    ASSERT_EQ( range.rangeSpecList[0].end.get(), 230 );
}

TEST( HttpRangeHeaderTest, validateByContentSize )
{
    nx_http::header::Range range;
    range.parse( "650-" );
    EXPECT_FALSE( range.empty() );
    EXPECT_TRUE( range.validateByContentSize(1000) );
    EXPECT_FALSE( range.validateByContentSize(0) );
    EXPECT_FALSE( range.validateByContentSize(650) );

    range.rangeSpecList.clear();
    range.parse( "100,200-230" );
    EXPECT_FALSE( range.empty() );
    EXPECT_FALSE( range.validateByContentSize(230) );
    EXPECT_TRUE( range.validateByContentSize(231) );
    EXPECT_TRUE( range.validateByContentSize(1000) );
    EXPECT_FALSE( range.validateByContentSize(150) );
    EXPECT_FALSE( range.validateByContentSize(100) );
    EXPECT_FALSE( range.validateByContentSize(200) );
    EXPECT_FALSE( range.validateByContentSize(0) );

    range.rangeSpecList.clear();
    range.parse( "200-230" );
    EXPECT_FALSE( range.empty() );
    EXPECT_FALSE( range.validateByContentSize(230) );
    EXPECT_TRUE( range.validateByContentSize(231) );
    EXPECT_TRUE( range.validateByContentSize(1000) );
    EXPECT_FALSE( range.validateByContentSize(150) );
    EXPECT_FALSE( range.validateByContentSize(0) );
}

TEST( HttpRangeHeaderTest, full )
{
    nx_http::header::Range range;
    range.parse( "650-" );
    EXPECT_FALSE( range.empty() );
    EXPECT_FALSE( range.full(1000) );

    range.rangeSpecList.clear();
    range.parse( "0-999" );
    EXPECT_FALSE( range.empty() );
    EXPECT_TRUE( range.full(1000) );
    EXPECT_TRUE( range.full(999) );
    EXPECT_FALSE( range.full(1001) );

    range.rangeSpecList.clear();
    range.parse( "0,1-999" );
    EXPECT_FALSE( range.empty() );
    EXPECT_TRUE( range.full(1000) );
    EXPECT_TRUE( range.full(999) );
    EXPECT_FALSE( range.full(1001) );

    range.rangeSpecList.clear();
    range.parse( "0" );
    EXPECT_FALSE( range.empty() );
    EXPECT_TRUE( range.full(1) );
    EXPECT_FALSE( range.full(2) );
}

TEST( HttpRangeHeaderTest, totalRangeLength )
{
    nx_http::header::Range range;
    range.parse( "650-" );
    EXPECT_EQ( range.totalRangeLength(1000), 350 );
    EXPECT_EQ( range.totalRangeLength(650), 0 );
    EXPECT_EQ( range.totalRangeLength(651), 1 );
    EXPECT_EQ( range.totalRangeLength(500), 0 );

    range.rangeSpecList.clear();
    range.parse( "0-999" );
    EXPECT_EQ( range.totalRangeLength(1000), 1000 );
    EXPECT_EQ( range.totalRangeLength(10000), 1000 );
    EXPECT_EQ( range.totalRangeLength(999), 999);
    EXPECT_EQ( range.totalRangeLength(500), 500);
    EXPECT_EQ( range.totalRangeLength(0), 0 );

    range.rangeSpecList.clear();
    range.parse( "0,1-999" );
    EXPECT_EQ( range.totalRangeLength(1000), 1000 );
    EXPECT_EQ( range.totalRangeLength(10000), 1000 );
    EXPECT_EQ( range.totalRangeLength(999), 999);
    EXPECT_EQ( range.totalRangeLength(0), 0 );

    range.rangeSpecList.clear();
    range.parse( "0" );
    EXPECT_EQ( range.totalRangeLength(1000), 1 );
    EXPECT_EQ( range.totalRangeLength(1), 1 );
    EXPECT_EQ( range.totalRangeLength(0), 0 );
}
