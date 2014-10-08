/**********************************************************
* 8 sep 2014
* a.kolesnikov
***********************************************************/

#include <gtest/gtest.h>

#include <utils/network/http/httptypes.h>


namespace nx_http
{
    namespace header
    {
        void PrintTo(const Via& val, ::std::ostream* os)
        {
            PrintTo( val.toString(), os );
        }

        bool operator==( const Via::ProxyEntry& left, const Via::ProxyEntry& right )
        {
            return left.protoName == right.protoName &&
                   left.protoVersion == right.protoVersion&&
                   left.receivedBy == right.receivedBy &&
                   left.comment == right.comment;
        }

        bool operator==( const Via& left, const Via& right )
        {
            return left.entries == right.entries;
        }
    }
}

//////////////////////////////////////////////
//   Range header tests
//////////////////////////////////////////////

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


//////////////////////////////////////////////
//   Via header tests
//////////////////////////////////////////////

TEST( HttpViaHeaderTest, parse )
{
    nx_http::header::Via via;
    EXPECT_TRUE( via.parse("1.0 fred, 1.1 nowhere.com (Apache/1.1)") );
    EXPECT_EQ( via.entries.size(), 2 );
    EXPECT_FALSE( via.entries[0].protoName );
    EXPECT_EQ( via.entries[0].protoVersion, QByteArray("1.0") );
    EXPECT_EQ( via.entries[0].receivedBy, QByteArray("fred") );
    EXPECT_TRUE( via.entries[0].comment.isEmpty() );
    EXPECT_EQ( via.entries[1].protoVersion, QByteArray("1.1") );
    EXPECT_EQ( via.entries[1].receivedBy, QByteArray("nowhere.com") );
    EXPECT_EQ( via.entries[1].comment, QByteArray("(Apache/1.1)") );


    via.entries.clear();
    EXPECT_TRUE( via.parse("1.0 ricky, 1.1 ethel, 1.1 fred, 1.0 lucy") );
    EXPECT_EQ( via.entries.size(), 4 );
    EXPECT_FALSE( via.entries[0].protoName );
    EXPECT_EQ( via.entries[0].protoVersion, QByteArray("1.0") );
    EXPECT_EQ( via.entries[0].receivedBy, QByteArray("ricky") );
    EXPECT_TRUE( via.entries[0].comment.isEmpty() );
    EXPECT_FALSE( via.entries[1].protoName );
    EXPECT_EQ( via.entries[1].protoVersion, QByteArray("1.1") );
    EXPECT_EQ( via.entries[1].receivedBy, QByteArray("ethel") );
    EXPECT_TRUE( via.entries[1].comment.isEmpty() );
    EXPECT_FALSE( via.entries[2].protoName );
    EXPECT_EQ( via.entries[2].protoVersion, QByteArray("1.1") );
    EXPECT_EQ( via.entries[2].receivedBy, QByteArray("fred") );
    EXPECT_TRUE( via.entries[2].comment.isEmpty() );
    EXPECT_FALSE( via.entries[3].protoName );
    EXPECT_EQ( via.entries[3].protoVersion, QByteArray("1.0") );
    EXPECT_EQ( via.entries[3].receivedBy, QByteArray("lucy") );
    EXPECT_TRUE( via.entries[3].comment.isEmpty() );


    via.entries.clear();
    EXPECT_TRUE( via.parse("HTTP/1.0 ricky") );
    EXPECT_EQ( via.entries.size(), 1 );
    EXPECT_EQ( via.entries[0].protoName, QByteArray("HTTP") );
    EXPECT_EQ( via.entries[0].protoVersion, QByteArray("1.0") );
    EXPECT_EQ( via.entries[0].receivedBy, QByteArray("ricky") );


    via.entries.clear();
    EXPECT_FALSE( via.parse("HTTP/1.0, hren") );


    via.entries.clear();
    EXPECT_TRUE( via.parse("") );


    via.entries.clear();
    EXPECT_FALSE( via.parse("h") );


    via.entries.clear();
    EXPECT_TRUE( via.parse("h g") );
    EXPECT_EQ( via.entries.size(), 1 );
    EXPECT_FALSE( via.entries[0].protoName );
    EXPECT_EQ( via.entries[0].protoVersion, QByteArray("h") );
    EXPECT_EQ( via.entries[0].receivedBy, QByteArray("g") );
    EXPECT_TRUE( via.entries[0].comment.isEmpty() );


    via.entries.clear();
    EXPECT_TRUE( via.parse("  h  g   ,    h   z   mm , p/v  ps") );
    EXPECT_EQ( via.entries.size(), 3 );
    EXPECT_FALSE( via.entries[0].protoName );
    EXPECT_EQ( via.entries[0].protoVersion, QByteArray("h") );
    EXPECT_EQ( via.entries[0].receivedBy, QByteArray("g") );
    EXPECT_TRUE( via.entries[0].comment.isEmpty() );
    EXPECT_FALSE( via.entries[1].protoName );
    EXPECT_EQ( via.entries[1].protoVersion, QByteArray("h") );
    EXPECT_EQ( via.entries[1].receivedBy, QByteArray("z") );
    EXPECT_EQ( via.entries[1].comment, QByteArray("mm ") );
    EXPECT_EQ( via.entries[2].protoName.get(), QByteArray("p") );
    EXPECT_EQ( via.entries[2].protoVersion, QByteArray("v") );
    EXPECT_EQ( via.entries[2].receivedBy, QByteArray("ps") );
    EXPECT_TRUE( via.entries[2].comment.isEmpty() );


    via.entries.clear();
    EXPECT_TRUE( via.parse("1.0 fred, 1.1 nowhere.com (Apache/1.1) Commanch Whooyanch") );
    EXPECT_EQ( via.entries.size(), 2 );
    EXPECT_FALSE( via.entries[0].protoName );
    EXPECT_EQ( via.entries[0].protoVersion, QByteArray("1.0") );
    EXPECT_EQ( via.entries[0].receivedBy, QByteArray("fred") );
    EXPECT_TRUE( via.entries[0].comment.isEmpty() );
    EXPECT_EQ( via.entries[1].protoVersion, QByteArray("1.1") );
    EXPECT_EQ( via.entries[1].receivedBy, QByteArray("nowhere.com") );
    EXPECT_EQ( via.entries[1].comment, QByteArray("(Apache/1.1) Commanch Whooyanch") );
}

TEST( HttpViaHeaderTest, toString )
{
    nx_http::header::Via via;
    nx_http::header::Via::ProxyEntry entry;
    entry.protoVersion = "1.0";
    entry.receivedBy = "{bla-bla-bla}";
    via.entries.push_back( entry );

    entry.protoName = "HTTP";
    entry.protoVersion = "1.0";
    entry.receivedBy = "{bla-bla-bla-bla}";
    via.entries.push_back( entry );

    entry.protoName = "HTTP";
    entry.protoVersion = "1.1";
    entry.receivedBy = "{blya-blya-blya-blya}";
    entry.comment = "qweasd123";
    via.entries.push_back( entry );

    EXPECT_EQ( via.toString(), QByteArray("1.0 {bla-bla-bla}, HTTP/1.0 {bla-bla-bla-bla}, HTTP/1.1 {blya-blya-blya-blya} qweasd123") );

    nx_http::header::Via via2;
    EXPECT_TRUE( via2.parse( via.toString() ) );
    EXPECT_EQ( via, via2 );
}
