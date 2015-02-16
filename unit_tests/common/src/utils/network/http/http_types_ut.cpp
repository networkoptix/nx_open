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

TEST( HttpHeaderTest, RangeHeader_parse )
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

TEST( HttpHeaderTest, RangeHeader_validateByContentSize )
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

TEST( HttpHeaderTest, RangeHeader_full )
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

TEST( HttpHeaderTest, RangeHeader_totalRangeLength )
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
//   Content-Range test
//////////////////////////////////////////////
TEST( HttpHeaderTest, ContentRange_toString )
{
    {
        nx_http::header::ContentRange contentRange;
        EXPECT_EQ( contentRange.toString(), "bytes 0-0/*" );
        EXPECT_EQ( contentRange.rangeLength(), 1 );

        contentRange.instanceLength = 240;
        EXPECT_EQ( contentRange.toString(), "bytes 0-239/240" );
        EXPECT_EQ( contentRange.rangeLength(), 240 );

        contentRange.rangeSpec.start = 100;
        EXPECT_EQ( contentRange.toString(), "bytes 100-239/240" );
        EXPECT_EQ( contentRange.rangeLength(), 140 );
    }

    {
        nx_http::header::ContentRange contentRange;
        contentRange.rangeSpec.start = 100;
        contentRange.rangeSpec.end = 249;
        EXPECT_EQ( contentRange.toString(), "bytes 100-249/*" );
        EXPECT_EQ( contentRange.rangeLength(), 150 );
    }

    {
        nx_http::header::ContentRange contentRange;
        contentRange.rangeSpec.start = 100;
        contentRange.rangeSpec.end = 249;
        contentRange.instanceLength = 500;
        EXPECT_EQ( contentRange.toString(), "bytes 100-249/500" );
        EXPECT_EQ( contentRange.rangeLength(), 150 );
    }

    {
        nx_http::header::ContentRange contentRange;
        contentRange.rangeSpec.start = 100;
        EXPECT_EQ( contentRange.toString(), "bytes 100-100/*" );
        EXPECT_EQ( contentRange.rangeLength(), 1 );
    }

    {
        nx_http::header::ContentRange contentRange;
        contentRange.rangeSpec.start = 100;
        contentRange.rangeSpec.end = 100;
        EXPECT_EQ( contentRange.toString(), "bytes 100-100/*" );
        EXPECT_EQ( contentRange.rangeLength(), 1 );
    }
}



//////////////////////////////////////////////
//   Via header tests
//////////////////////////////////////////////

TEST( HttpHeaderTest, Via_parse )
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

TEST( HttpHeaderTest, Via_toString )
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



//////////////////////////////////////////////
//   Via header tests
//////////////////////////////////////////////

TEST( HttpHeaderTest, AcceptEncoding_parse )
{
    {
        nx_http::header::AcceptEncodingHeader acceptEncoding( "gzip, deflate, sdch" );
        ASSERT_TRUE( acceptEncoding.encodingIsAllowed("gzip") );
        ASSERT_TRUE( acceptEncoding.encodingIsAllowed("deflate") );
        ASSERT_TRUE( acceptEncoding.encodingIsAllowed("sdch") );
        ASSERT_TRUE( acceptEncoding.encodingIsAllowed("identity") );
        ASSERT_FALSE( acceptEncoding.encodingIsAllowed("qweasd123") );
    }

    {
        nx_http::header::AcceptEncodingHeader acceptEncoding( "gzip, deflate, sdch, identity;q=0" );
        ASSERT_TRUE( acceptEncoding.encodingIsAllowed("gzip") );
        ASSERT_TRUE( acceptEncoding.encodingIsAllowed("deflate") );
        ASSERT_TRUE( acceptEncoding.encodingIsAllowed("sdch") );
        ASSERT_FALSE( acceptEncoding.encodingIsAllowed("identity") );
        ASSERT_FALSE( acceptEncoding.encodingIsAllowed("qweasd123") );
    }

    {
        nx_http::header::AcceptEncodingHeader acceptEncoding( "" );
        ASSERT_FALSE( acceptEncoding.encodingIsAllowed("gzip") );
        ASSERT_TRUE( acceptEncoding.encodingIsAllowed("identity") );
    }

    {
        nx_http::header::AcceptEncodingHeader acceptEncoding( "gzip;q=0, deflate;q=0.5, sdch" );
        ASSERT_FALSE( acceptEncoding.encodingIsAllowed("gzip") );
        ASSERT_TRUE( acceptEncoding.encodingIsAllowed("deflate") );
        ASSERT_TRUE( acceptEncoding.encodingIsAllowed("sdch") );
        ASSERT_TRUE( acceptEncoding.encodingIsAllowed("identity") );
    }

    {
        nx_http::header::AcceptEncodingHeader acceptEncoding( "gzip;q=0, deflate;q=0.5, sdch,*" );
        ASSERT_FALSE( acceptEncoding.encodingIsAllowed("gzip") );
        ASSERT_TRUE( acceptEncoding.encodingIsAllowed("deflate") );
        ASSERT_TRUE( acceptEncoding.encodingIsAllowed("sdch") );
        ASSERT_TRUE( acceptEncoding.encodingIsAllowed("identity") );
        ASSERT_TRUE( acceptEncoding.encodingIsAllowed("qweasd123") );
    }

    {
        nx_http::header::AcceptEncodingHeader acceptEncoding( "gzip;q=0, deflate;q=0.5, sdch,*;q=0.0" );
        ASSERT_FALSE( acceptEncoding.encodingIsAllowed("gzip") );
        ASSERT_TRUE( acceptEncoding.encodingIsAllowed("deflate") );
        ASSERT_TRUE( acceptEncoding.encodingIsAllowed("sdch") );
        ASSERT_FALSE( acceptEncoding.encodingIsAllowed("identity") );
        ASSERT_FALSE( acceptEncoding.encodingIsAllowed("qweasd123") );
    }
}


//////////////////////////////////////////////
//   nx_http::Request
//////////////////////////////////////////////

static const nx::Buffer HTTP_REQUEST(
    "PLAY rtsp://192.168.0.25:7001/00-1A-07-0A-3A-88 RTSP/1.0\r\n"
    "CSeq: 2\r\n"
    "Range: npt=1423440107300000-1423682432925000\r\n"
    "Scale: 1\r\n"
    "x-guid: {ff69b2e0-1f1e-4512-8eb9-4d20b33587dc}\r\n"
    "Session: \r\n"
    "User-Agent: Network Optix\r\n"
    "x-play-now: true\r\n"
    "x-media-step: 9693025000\r\n"
    "Authorization: Basic YWRtaW46MTIz\r\n"
    "x-server-guid: {d1a49afe-a2a2-990a-2c54-b77e768e6ad2}\r\n"
    "x-media-quality: low\r\n"
    "\r\n" );

TEST( HttpHeaderTest, Request_parse )
{
    nx_http::Request request;
    ASSERT_TRUE( request.parse( HTTP_REQUEST ) );
    ASSERT_EQ( nx_http::getHeaderValue( request.headers, "x-media-step" ).toLongLong(), 9693025000LL );
}
