/**********************************************************
* 8 sep 2014
* a.kolesnikov
***********************************************************/

#include <gtest/gtest.h>

#include <nx/network/http/http_types.h>
#include <nx/utils/string.h>


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
    ASSERT_EQ( range.rangeSpecList.size(), 1U );
    ASSERT_EQ( range.rangeSpecList[0].start, 650U );
    ASSERT_FALSE( static_cast< bool >(range.rangeSpecList[0].end ) );

    range.rangeSpecList.clear();
    range.parse( "100,200-230" );
    ASSERT_EQ( range.rangeSpecList.size(), 2U );
    ASSERT_EQ( range.rangeSpecList[0].start, 100U );
    ASSERT_TRUE( static_cast< bool >( range.rangeSpecList[0].end ) );
    ASSERT_EQ( range.rangeSpecList[0].end.get(), 100U );
    ASSERT_EQ( range.rangeSpecList[1].start, 200U );
    ASSERT_EQ( range.rangeSpecList[1].end.get(), 230U );

    range.rangeSpecList.clear();
    range.parse( "200-230" );
    ASSERT_EQ( range.rangeSpecList.size(), 1U );
    ASSERT_EQ( range.rangeSpecList[0].start, 200U );
    ASSERT_EQ( range.rangeSpecList[0].end.get(), 230U );
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
    EXPECT_EQ( range.totalRangeLength(1000), 350U );
    EXPECT_EQ( range.totalRangeLength(650), 0U );
    EXPECT_EQ( range.totalRangeLength(651), 1U );
    EXPECT_EQ( range.totalRangeLength(500), 0U );

    range.rangeSpecList.clear();
    range.parse( "0-999" );
    EXPECT_EQ( range.totalRangeLength(1000), 1000U );
    EXPECT_EQ( range.totalRangeLength(10000), 1000U );
    EXPECT_EQ( range.totalRangeLength(999), 999U);
    EXPECT_EQ( range.totalRangeLength(500), 500U);
    EXPECT_EQ( range.totalRangeLength(0), 0U );

    range.rangeSpecList.clear();
    range.parse( "0,1-999" );
    EXPECT_EQ( range.totalRangeLength(1000), 1000U );
    EXPECT_EQ( range.totalRangeLength(10000), 1000U );
    EXPECT_EQ( range.totalRangeLength(999), 999U);
    EXPECT_EQ( range.totalRangeLength(0), 0U );

    range.rangeSpecList.clear();
    range.parse( "0" );
    EXPECT_EQ( range.totalRangeLength(1000), 1U );
    EXPECT_EQ( range.totalRangeLength(1), 1U );
    EXPECT_EQ( range.totalRangeLength(0), 0U );
}



//////////////////////////////////////////////
//   Content-Range test
//////////////////////////////////////////////
TEST( HttpHeaderTest, ContentRange_toString )
{
    {
        nx_http::header::ContentRange contentRange;
        EXPECT_EQ( contentRange.toString(), "bytes 0-0/*" );
        EXPECT_EQ( contentRange.rangeLength(), 1U );

        contentRange.instanceLength = 240;
        EXPECT_EQ( contentRange.toString(), "bytes 0-239/240" );
        EXPECT_EQ( contentRange.rangeLength(), 240U );

        contentRange.rangeSpec.start = 100;
        EXPECT_EQ( contentRange.toString(), "bytes 100-239/240" );
        EXPECT_EQ( contentRange.rangeLength(), 140U );
    }

    {
        nx_http::header::ContentRange contentRange;
        contentRange.rangeSpec.start = 100;
        contentRange.rangeSpec.end = 249;
        EXPECT_EQ( contentRange.toString(), "bytes 100-249/*" );
        EXPECT_EQ( contentRange.rangeLength(), 150U );
    }

    {
        nx_http::header::ContentRange contentRange;
        contentRange.rangeSpec.start = 100;
        contentRange.rangeSpec.end = 249;
        contentRange.instanceLength = 500;
        EXPECT_EQ( contentRange.toString(), "bytes 100-249/500" );
        EXPECT_EQ( contentRange.rangeLength(), 150U );
    }

    {
        nx_http::header::ContentRange contentRange;
        contentRange.rangeSpec.start = 100;
        EXPECT_EQ( contentRange.toString(), "bytes 100-100/*" );
        EXPECT_EQ( contentRange.rangeLength(), 1U );
    }

    {
        nx_http::header::ContentRange contentRange;
        contentRange.rangeSpec.start = 100;
        contentRange.rangeSpec.end = 100;
        EXPECT_EQ( contentRange.toString(), "bytes 100-100/*" );
        EXPECT_EQ( contentRange.rangeLength(), 1U );
    }
}


//////////////////////////////////////////////
//   Via header tests
//////////////////////////////////////////////

TEST( HttpHeaderTest, Via_parse )
{
    nx_http::header::Via via;
    EXPECT_TRUE( via.parse("1.0 fred, 1.1 nowhere.com (Apache/1.1)") );
    EXPECT_EQ( via.entries.size(), 2U );
    EXPECT_FALSE( via.entries[0].protoName );
    EXPECT_EQ( via.entries[0].protoVersion, QByteArray("1.0") );
    EXPECT_EQ( via.entries[0].receivedBy, QByteArray("fred") );
    EXPECT_TRUE( via.entries[0].comment.isEmpty() );
    EXPECT_EQ( via.entries[1].protoVersion, QByteArray("1.1") );
    EXPECT_EQ( via.entries[1].receivedBy, QByteArray("nowhere.com") );
    EXPECT_EQ( via.entries[1].comment, QByteArray("(Apache/1.1)") );


    via.entries.clear();
    EXPECT_TRUE( via.parse("1.0 ricky, 1.1 ethel, 1.1 fred, 1.0 lucy") );
    EXPECT_EQ( via.entries.size(), 4U );
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
    EXPECT_EQ( via.entries.size(), 1U );
    EXPECT_EQ( via.entries[0].protoName.get(), QByteArray("HTTP") );
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
    EXPECT_EQ( via.entries.size(), 1U );
    EXPECT_FALSE( via.entries[0].protoName );
    EXPECT_EQ( via.entries[0].protoVersion, QByteArray("h") );
    EXPECT_EQ( via.entries[0].receivedBy, QByteArray("g") );
    EXPECT_TRUE( via.entries[0].comment.isEmpty() );


    via.entries.clear();
    EXPECT_TRUE( via.parse("  h  g   ,    h   z   mm , p/v  ps") );
    EXPECT_EQ( via.entries.size(), 3U );
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
    EXPECT_EQ( via.entries.size(), 2U );
    EXPECT_FALSE( via.entries[0].protoName );
    EXPECT_EQ( via.entries[0].protoVersion, QByteArray("1.0") );
    EXPECT_EQ( via.entries[0].receivedBy, QByteArray("fred") );
    EXPECT_TRUE( via.entries[0].comment.isEmpty() );
    EXPECT_EQ( via.entries[1].protoVersion, QByteArray("1.1") );
    EXPECT_EQ( via.entries[1].receivedBy, QByteArray("nowhere.com") );
    EXPECT_EQ( via.entries[1].comment, QByteArray("(Apache/1.1) Commanch Whooyanch") );



    via.entries.clear();
    EXPECT_TRUE( via.parse( "1.1 {47bf37a0-72a6-2890-b967-5da9c390d28a}" ) );
    EXPECT_EQ( via.entries.size(), 1U );
    EXPECT_FALSE( via.entries[0].protoName );
    EXPECT_EQ( via.entries[0].protoVersion, QByteArray( "1.1" ) );
    EXPECT_EQ( via.entries[0].receivedBy, QByteArray( "{47bf37a0-72a6-2890-b967-5da9c390d28a}" ) );
    EXPECT_TRUE( via.entries[0].comment.isEmpty() );



    via.entries.clear();
    EXPECT_FALSE( via.parse( " " ) );
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
//   Accept-Encoding header tests
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

    {
        nx_http::header::AcceptEncodingHeader acceptEncoding( "gzip;q=1.0, identity;q=0.5, *;q=0" );
        ASSERT_TRUE( acceptEncoding.encodingIsAllowed("gzip") );
        ASSERT_FALSE( acceptEncoding.encodingIsAllowed("deflate") );
        ASSERT_TRUE( acceptEncoding.encodingIsAllowed("identity") );
        ASSERT_FALSE( acceptEncoding.encodingIsAllowed("qweasd123") );
    }
}


//////////////////////////////////////////////
//   nx_http::RequestLine
//////////////////////////////////////////////

TEST( HttpRequestTest, RequestLine_parse )
{
    {
        nx_http::RequestLine requestLine;
        ASSERT_TRUE( requestLine.parse( nx_http::BufferType( "GET /hren/hren/hren?hren=hren&hren HTTP/1.0" ) ) );
        ASSERT_EQ( requestLine.method, nx_http::BufferType("GET") );
        ASSERT_EQ( requestLine.url, QUrl("/hren/hren/hren?hren=hren&hren") );
        ASSERT_EQ( requestLine.version, nx_http::http_1_0 );
    }

    {
        nx_http::RequestLine requestLine;
        ASSERT_TRUE( requestLine.parse( nx_http::BufferType( "  PUT   /abc?def=ghi&jkl   HTTP/1.1" ) ) );
        ASSERT_EQ( requestLine.method, nx_http::BufferType("PUT") );
        ASSERT_EQ( requestLine.url, QUrl("/abc?def=ghi&jkl") );
        ASSERT_EQ( requestLine.version, nx_http::http_1_1 );
    }

    {
        nx_http::RequestLine requestLine;
        ASSERT_FALSE( requestLine.parse( nx_http::BufferType( "GET    HTTP/1.1" ) ) );
        ASSERT_FALSE( requestLine.parse( nx_http::BufferType() ) );
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

TEST( HttpRequestTest, Request_parse )
{
    nx_http::Request request;
    ASSERT_TRUE( request.parse( HTTP_REQUEST ) );
    ASSERT_EQ( nx_http::getHeaderValue( request.headers, "x-media-step" ).toLongLong(), 9693025000LL );
}


//////////////////////////////////////////////
//   WWWAuthenticate header tests
//////////////////////////////////////////////

TEST( HttpHeaderTest, WWWAuthenticate_parse )
{
    {
        static const char testData[] = "Digest realm=\"AXIS_ACCC8E338EDF\", nonce=\"p65VeyEWBQA=0b7e4955ab1d73d00a4b903c19d91c67931ef7ad\", algorithm=MD5, qop=\"auth\"";

        nx_http::header::WWWAuthenticate auth;
        ASSERT_TRUE( auth.parse( QByteArray::fromRawData(testData, sizeof(testData)-1) ) );
        ASSERT_EQ( auth.authScheme, nx_http::header::AuthScheme::digest );
        ASSERT_EQ( auth.params.size(), 4 );
        ASSERT_EQ( auth.params["realm"], "AXIS_ACCC8E338EDF" );
        ASSERT_EQ( auth.params["algorithm"], "MD5" );
        ASSERT_EQ( auth.params["qop"], "auth" );
        ASSERT_EQ( auth.params["nonce"], "p65VeyEWBQA=0b7e4955ab1d73d00a4b903c19d91c67931ef7ad" );
    }

    {
        static const char testData[] = "Digest realm=AXIS_ACCC8E338EDF, nonce=\"p65VeyEWBQA=0b7e4955ab1d73d00a4b903c19d91c67931ef7ad\", algorithm=MD5, qop=auth";

        nx_http::header::WWWAuthenticate auth;
        ASSERT_TRUE( auth.parse( QByteArray::fromRawData(testData, sizeof(testData)-1) ) );
        ASSERT_EQ( auth.authScheme, nx_http::header::AuthScheme::digest );
        ASSERT_EQ( auth.params.size(), 4 );
        ASSERT_EQ( auth.params["realm"], "AXIS_ACCC8E338EDF" );
        ASSERT_EQ( auth.params["algorithm"], "MD5" );
        ASSERT_EQ( auth.params["qop"], "auth" );
        ASSERT_EQ( auth.params["nonce"], "p65VeyEWBQA=0b7e4955ab1d73d00a4b903c19d91c67931ef7ad" );
    }

    //TODO #ak uncomment and fix!
    //{
    //    static const char testData[] = "Digest realm=AXIS_ACCC8E338EDF, nonce=p65VeyEWBQA=0b7e4955ab1d73d00a4b903c19d91c67931ef7ad, algorithm=MD5, qop=auth";

    //    nx_http::header::WWWAuthenticate auth;
    //    ASSERT_FALSE( auth.parse( QByteArray::fromRawData(testData, sizeof(testData)-1) ) );
    //}
}

TEST( HttpHeaderTest, Authorization_parse )
{
    {
        static const char testData[] =
            "Digest username=\"Mufasa\","
                "realm=\"testrealm@host.com\","
                "nonce=\"dcd98b7102dd2f0e8b11d0f600bfb0c093\","
                "uri=\"/dir/index.html\","
                "qop=auth,"
                "nc=00000001,"
                "cnonce=\"0a4f113b\","
                "response=\"6629fae49393a05397450978507c4ef1\","
                "opaque=\"5ccc069c403ebaf9f0171e9517f40e41\"";
        nx_http::header::Authorization auth;
        ASSERT_TRUE( auth.parse( QByteArray::fromRawData( testData, sizeof( testData ) - 1 ) ) );
        ASSERT_EQ( auth.authScheme, nx_http::header::AuthScheme::digest );
        ASSERT_EQ( auth.userid(), "Mufasa" );
        ASSERT_EQ( auth.digest->params["username"], "Mufasa" );
        ASSERT_EQ( auth.digest->params["realm"], "testrealm@host.com" );
        ASSERT_EQ( auth.digest->params["nonce"], "dcd98b7102dd2f0e8b11d0f600bfb0c093" );
        ASSERT_EQ( auth.digest->params["uri"], "/dir/index.html" );
        ASSERT_EQ( auth.digest->params["qop"], "auth" );
        ASSERT_EQ( auth.digest->params["nc"], "00000001" );
        ASSERT_EQ( auth.digest->params["cnonce"], "0a4f113b" );
        ASSERT_EQ( auth.digest->params["response"], "6629fae49393a05397450978507c4ef1" );
        ASSERT_EQ( auth.digest->params["opaque"], "5ccc069c403ebaf9f0171e9517f40e41" );
    }

    {
        static const char testData[] = "Digest realm=AXIS_ACCC8E338EDF, nonce=\"p65VeyEWBQA=0b7e4955ab1d73d00a4b903c19d91c67931ef7ad\", algorithm=MD5, qop=auth";

        nx_http::header::Authorization auth;
        ASSERT_TRUE( auth.parse( QByteArray::fromRawData( testData, sizeof( testData ) - 1 ) ) );
        ASSERT_EQ( auth.authScheme, nx_http::header::AuthScheme::digest );
        ASSERT_EQ( auth.digest->params.size(), 4 );
        ASSERT_EQ( auth.digest->params["realm"], "AXIS_ACCC8E338EDF" );
        ASSERT_EQ( auth.digest->params["algorithm"], "MD5" );
        ASSERT_EQ( auth.digest->params["qop"], "auth" );
        ASSERT_EQ( auth.digest->params["nonce"], "p65VeyEWBQA=0b7e4955ab1d73d00a4b903c19d91c67931ef7ad" );
    }

    {
        static const char testData[] = "Basic QWxhZGRpbjpvcGVuIHNlc2FtZQ==";

        nx_http::header::Authorization auth;
        ASSERT_TRUE( auth.parse( QByteArray::fromRawData( testData, sizeof( testData ) - 1 ) ) );
        ASSERT_EQ( auth.authScheme, nx_http::header::AuthScheme::basic );
        ASSERT_EQ( auth.basic->userid, "Aladdin" );
        ASSERT_EQ( auth.basic->password, "open sesame" );
    }
}

TEST( HttpHeaderTest, parseDigestAuthParams )
{
    {
        static const char testData[] =
            "X-runtime-guid={9968ed68-0a83-47f6-adb6-af50cd9bcad2}; csrftoken=JWGZ9bRtEBNoa7kELHWx0ZJdpqqMgbVp; "
            "X-runtime-guid={d189fb72-54d7-4e7a-b2aa-90712e5f3a55}; Authorization=Digest; nonce=51d1c3e51d750; "
            "realm=networkoptix; auth=YWRtaW46NTFkMWMzZTUxZDc1MDo2YjA5YmJhYjFmMGY4NDE3ZmI1ZmYwNzcyZGE0MmJjNA%3D%3D; "
            "auth_rtsp=YWRtaW46NTFkMWMzZTUxZDc1MDo3M2I1MDVhYTMzNjcyNDZmNDEzYWQ2ZThkZjg1MDRkZQ%3D%3D; "
            "response=6b09bbab1f0f8417fb5ff0772da42bc4; username=admin";
        QMap<nx_http::BufferType, nx_http::BufferType> params;
        nx::utils::parseNameValuePairs(
            QByteArray::fromRawData( testData, sizeof( testData ) - 1 ),
            ';',
            &params);
        ASSERT_EQ( params["auth"], "YWRtaW46NTFkMWMzZTUxZDc1MDo2YjA5YmJhYjFmMGY4NDE3ZmI1ZmYwNzcyZGE0MmJjNA%3D%3D" );
    }
}

TEST(HttpHeaderTest, KeepAlive_parse)
{
    {
        static const char testData[] = "timeout=60, max=100";

        nx_http::header::KeepAlive keepAlive;
        ASSERT_TRUE(keepAlive.parse(testData));
        ASSERT_EQ(std::chrono::seconds(60), keepAlive.timeout);
        ASSERT_TRUE(static_cast<bool>(keepAlive.max));
        ASSERT_EQ(100, keepAlive.max.get());
    }

    {
        static const char testData[] = "  timeout=60 ";

        nx_http::header::KeepAlive keepAlive;
        ASSERT_TRUE(keepAlive.parse(testData));
        ASSERT_EQ(std::chrono::seconds(60), keepAlive.timeout);
        ASSERT_FALSE(static_cast<bool>(keepAlive.max));
    }

    {
        static const char testData[] = "  timeout=60  , max=100  ";

        nx_http::header::KeepAlive keepAlive;
        ASSERT_TRUE(keepAlive.parse(testData));
        ASSERT_EQ(std::chrono::seconds(60), keepAlive.timeout);
        ASSERT_TRUE(static_cast<bool>(keepAlive.max));
        ASSERT_EQ(100, keepAlive.max.get());
    }

    {
        static const char testData[] = " max=100  ";

        nx_http::header::KeepAlive keepAlive;
        ASSERT_FALSE(keepAlive.parse(testData));
    }
}

//-------------------------------------------------------------------------------------------------
// Server

class HttpHeaderServer:
    public ::testing::Test
{
public:

protected:
    void test(
        bool isValid,
        const nx_http::header::Server& serverHeader,
        nx_http::StringType serializedValue)
    {
        if (isValid)
            testSerialization(serverHeader, serializedValue);

        testParsing(isValid, serverHeader, serializedValue);
    }

    void testParsing(
        bool isValid,
        const nx_http::header::Server& serverHeader,
        nx_http::StringType serializedValue)
    {
        using namespace nx_http::header;

        Server headerToParse;
        ASSERT_EQ(isValid, headerToParse.parse(serializedValue));
        if (isValid)
        {
            ASSERT_EQ(serverHeader, headerToParse);
        }
    }

    void testSerialization(
        const nx_http::header::Server& serverHeader,
        nx_http::StringType expectedSerializedValue)
    {
        const auto result = serverHeader.toString();
        ASSERT_TRUE(result.startsWith(expectedSerializedValue));
    }
};

TEST_F(HttpHeaderServer, single_product_without_comment)
{
    nx_http::header::Server serverHeader;
    serverHeader.products.clear();
    serverHeader.products.push_back(
        nx_http::header::Server::Product{
            "ProductName", nx::utils::SoftwareVersion("1.2.3.4"), ""});
    test(true, serverHeader, "ProductName/1.2.3.4");
}

TEST_F(HttpHeaderServer, single_product_without_version)
{
    nx_http::header::Server serverHeader;
    serverHeader.products.clear();
    serverHeader.products.push_back(
        nx_http::header::Server::Product{"ProductName", boost::none, ""});
    test(true, serverHeader, "ProductName");
}

TEST_F(HttpHeaderServer, multile_products)
{
    nx_http::header::Server serverHeader;
    serverHeader.products.clear();
    serverHeader.products.push_back(
        nx_http::header::Server::Product{
        "Product1", nx::utils::SoftwareVersion("1.2.3.4"), "" });
    serverHeader.products.push_back(
        nx_http::header::Server::Product{
        "Product2", nx::utils::SoftwareVersion("5.6.7.8"), "comment2" });
    test(true, serverHeader, "Product1/1.2.3.4 Product2/5.6.7.8 (comment2)");
}

TEST_F(HttpHeaderServer, empty_string)
{
    nx_http::header::Server serverHeader;
    serverHeader.products.clear();
    test(false, serverHeader, "");
}

TEST_F(HttpHeaderServer, empty_product_first)
{
    nx_http::header::Server serverHeader;
    test(false, serverHeader, "/1.2.3.4");
}

TEST_F(HttpHeaderServer, empty_product_not_first)
{
    nx_http::header::Server serverHeader;
    test(false, serverHeader, "Product1/1.2.3.4 /5.6.7.8 (comment2)");
}

TEST(Http_readHeader, int_value)
{
    nx_http::HttpHeaders headers;
    headers.emplace("Header1", "str");
    headers.emplace("Header2", "777");

    int value = 0;
    
    ASSERT_TRUE(nx_http::readHeader(headers, "Header2", &value));
    ASSERT_EQ(777, value);

    ASSERT_TRUE(nx_http::readHeader(headers, "Header1", &value));
    ASSERT_EQ(0, value);
}

TEST(Http_readHeader, missing_header)
{
    nx_http::HttpHeaders headers;
    int value = 0;
    ASSERT_FALSE(nx_http::readHeader(headers, "Header3", &value));
}
