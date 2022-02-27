// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/network/http/http_types.h>
#include <nx/utils/test_support/custom_gtest_printers.h>
#include <nx/utils/string.h>

namespace nx::network::http::header {

void PrintTo(const Via& val, ::std::ostream* os)
{
    *os << val.toString();
}

bool operator==(const Via::ProxyEntry& left, const Via::ProxyEntry& right)
{
    return left.protoName == right.protoName &&
        left.protoVersion == right.protoVersion &&
        left.receivedBy == right.receivedBy &&
        left.comment == right.comment;
}

bool operator==(const Via& left, const Via& right)
{
    return left.entries == right.entries;
}

} // namespace nx::network::http::header

namespace nx::network::http::test {

//-------------------------------------------------------------------------------------------------
// Common

TEST(HttpTypesHeader, read_int_value)
{
    HttpHeaders headers;
    headers.emplace("Header1", "str");
    headers.emplace("Header2", "777");

    int value = 0;

    ASSERT_TRUE(readHeader(headers, "Header2", &value));
    ASSERT_EQ(777, value);

    ASSERT_TRUE(readHeader(headers, "Header1", &value));
    ASSERT_EQ(0, value);
}

TEST(HttpTypesHeader, read_missing_header)
{
    HttpHeaders headers;
    int value = 0;
    ASSERT_FALSE(readHeader(headers, "Header3", &value));
}

TEST(HttpTypesHeader, parseHeader)
{
    ASSERT_EQ(HttpHeader("Name", "Value"), parseHeader("Name: Value"));
    ASSERT_EQ(HttpHeader("Name", "Value"), parseHeader(" Name:Value "));
    ASSERT_EQ(HttpHeader("Name", ""), parseHeader("Name:"));
    ASSERT_EQ(HttpHeader("", "Value"), parseHeader(":Value"));
    ASSERT_EQ(HttpHeader("", ""), parseHeader(":"));
    ASSERT_EQ(HttpHeader("", ""), parseHeader(""));
}

TEST(HttpTypesHeader, serializeHeaders)
{
    {
        nx::Buffer buffer;
        serializeHeaders({{"Content-Length", "1024"}, {"Content-Type", "application/json"}}, &buffer);
        ASSERT_EQ(
            "Content-Length: 1024\r\nContent-Type: application/json\r\n",
            buffer);
    }

    {
        nx::Buffer buffer;
        serializeHeaders({}, &buffer);
        ASSERT_EQ("", buffer);
    }
}

//-------------------------------------------------------------------------------------------------
// Range header tests.

TEST(HttpTypesHeaderRange, parse)
{
    header::Range range;
    range.parse("650-");
    ASSERT_EQ(range.rangeSpecList.size(), 1U);
    ASSERT_EQ(range.rangeSpecList[0].start, 650U);
    ASSERT_FALSE(static_cast< bool >(range.rangeSpecList[0].end));

    range.rangeSpecList.clear();
    range.parse("100,200-230");
    ASSERT_EQ(range.rangeSpecList.size(), 2U);
    ASSERT_EQ(range.rangeSpecList[0].start, 100U);
    ASSERT_TRUE(static_cast< bool >(range.rangeSpecList[0].end));
    ASSERT_EQ(*range.rangeSpecList[0].end, 100U);
    ASSERT_EQ(range.rangeSpecList[1].start, 200U);
    ASSERT_EQ(*range.rangeSpecList[1].end, 230U);

    range.rangeSpecList.clear();
    range.parse("200-230");
    ASSERT_EQ(range.rangeSpecList.size(), 1U);
    ASSERT_EQ(range.rangeSpecList[0].start, 200U);
    ASSERT_EQ(*range.rangeSpecList[0].end, 230U);
}

TEST(HttpTypesHeaderRange, validateByContentSize)
{
    header::Range range;
    range.parse("650-");
    EXPECT_FALSE(range.empty());
    EXPECT_TRUE(range.validateByContentSize(1000));
    EXPECT_FALSE(range.validateByContentSize(0));
    EXPECT_FALSE(range.validateByContentSize(650));

    range.rangeSpecList.clear();
    range.parse("100,200-230");
    EXPECT_FALSE(range.empty());
    EXPECT_FALSE(range.validateByContentSize(230));
    EXPECT_TRUE(range.validateByContentSize(231));
    EXPECT_TRUE(range.validateByContentSize(1000));
    EXPECT_FALSE(range.validateByContentSize(150));
    EXPECT_FALSE(range.validateByContentSize(100));
    EXPECT_FALSE(range.validateByContentSize(200));
    EXPECT_FALSE(range.validateByContentSize(0));

    range.rangeSpecList.clear();
    range.parse("200-230");
    EXPECT_FALSE(range.empty());
    EXPECT_FALSE(range.validateByContentSize(230));
    EXPECT_TRUE(range.validateByContentSize(231));
    EXPECT_TRUE(range.validateByContentSize(1000));
    EXPECT_FALSE(range.validateByContentSize(150));
    EXPECT_FALSE(range.validateByContentSize(0));
}

TEST(HttpTypesHeaderRange, full)
{
    header::Range range;
    range.parse("650-");
    EXPECT_FALSE(range.empty());
    EXPECT_FALSE(range.full(1000));

    range.rangeSpecList.clear();
    range.parse("0-999");
    EXPECT_FALSE(range.empty());
    EXPECT_TRUE(range.full(1000));
    EXPECT_TRUE(range.full(999));
    EXPECT_FALSE(range.full(1001));

    range.rangeSpecList.clear();
    range.parse("0,1-999");
    EXPECT_FALSE(range.empty());
    EXPECT_TRUE(range.full(1000));
    EXPECT_TRUE(range.full(999));
    EXPECT_FALSE(range.full(1001));

    range.rangeSpecList.clear();
    range.parse("0");
    EXPECT_FALSE(range.empty());
    EXPECT_TRUE(range.full(1));
    EXPECT_FALSE(range.full(2));
}

TEST(HttpTypesHeaderRange, totalRangeLength)
{
    header::Range range;
    range.parse("650-");
    EXPECT_EQ(range.totalRangeLength(1000), 350U);
    EXPECT_EQ(range.totalRangeLength(650), 0U);
    EXPECT_EQ(range.totalRangeLength(651), 1U);
    EXPECT_EQ(range.totalRangeLength(500), 0U);

    range.rangeSpecList.clear();
    range.parse("0-999");
    EXPECT_EQ(range.totalRangeLength(1000), 1000U);
    EXPECT_EQ(range.totalRangeLength(10000), 1000U);
    EXPECT_EQ(range.totalRangeLength(999), 999U);
    EXPECT_EQ(range.totalRangeLength(500), 500U);
    EXPECT_EQ(range.totalRangeLength(0), 0U);

    range.rangeSpecList.clear();
    range.parse("0,1-999");
    EXPECT_EQ(range.totalRangeLength(1000), 1000U);
    EXPECT_EQ(range.totalRangeLength(10000), 1000U);
    EXPECT_EQ(range.totalRangeLength(999), 999U);
    EXPECT_EQ(range.totalRangeLength(0), 0U);

    range.rangeSpecList.clear();
    range.parse("0");
    EXPECT_EQ(range.totalRangeLength(1000), 1U);
    EXPECT_EQ(range.totalRangeLength(1), 1U);
    EXPECT_EQ(range.totalRangeLength(0), 0U);
}

//-------------------------------------------------------------------------------------------------
// Content-Range test.

TEST(HttpTypesHeaderContentRange, toString)
{
    {
        header::ContentRange contentRange;
        EXPECT_EQ(contentRange.toString(), "bytes 0-0/*");
        EXPECT_EQ(contentRange.rangeLength(), 1U);

        contentRange.instanceLength = 240;
        EXPECT_EQ(contentRange.toString(), "bytes 0-239/240");
        EXPECT_EQ(contentRange.rangeLength(), 240U);

        contentRange.rangeSpec.start = 100;
        EXPECT_EQ(contentRange.toString(), "bytes 100-239/240");
        EXPECT_EQ(contentRange.rangeLength(), 140U);
    }

    {
        header::ContentRange contentRange;
        contentRange.rangeSpec.start = 100;
        contentRange.rangeSpec.end = 249;
        EXPECT_EQ(contentRange.toString(), "bytes 100-249/*");
        EXPECT_EQ(contentRange.rangeLength(), 150U);
    }

    {
        header::ContentRange contentRange;
        contentRange.rangeSpec.start = 100;
        contentRange.rangeSpec.end = 249;
        contentRange.instanceLength = 500;
        EXPECT_EQ(contentRange.toString(), "bytes 100-249/500");
        EXPECT_EQ(contentRange.rangeLength(), 150U);
    }

    {
        header::ContentRange contentRange;
        contentRange.rangeSpec.start = 100;
        EXPECT_EQ(contentRange.toString(), "bytes 100-100/*");
        EXPECT_EQ(contentRange.rangeLength(), 1U);
    }

    {
        header::ContentRange contentRange;
        contentRange.rangeSpec.start = 100;
        contentRange.rangeSpec.end = 100;
        EXPECT_EQ(contentRange.toString(), "bytes 100-100/*");
        EXPECT_EQ(contentRange.rangeLength(), 1U);
    }
}

//-------------------------------------------------------------------------------------------------
// Via header tests.

TEST(HttpTypesHeaderVia, parse)
{
    header::Via via;
    EXPECT_TRUE(via.parse("1.0 fred, 1.1 nowhere.com (Apache/1.1)"));
    EXPECT_EQ(via.entries.size(), 2U);
    EXPECT_FALSE(via.entries[0].protoName);
    EXPECT_EQ(via.entries[0].protoVersion, "1.0");
    EXPECT_EQ(via.entries[0].receivedBy, "fred");
    EXPECT_TRUE(via.entries[0].comment.empty());
    EXPECT_EQ(via.entries[1].protoVersion, "1.1");
    EXPECT_EQ(via.entries[1].receivedBy, "nowhere.com");
    EXPECT_EQ(via.entries[1].comment, "(Apache/1.1)");


    via.entries.clear();
    EXPECT_TRUE(via.parse("1.0 ricky, 1.1 ethel, 1.1 fred, 1.0 lucy"));
    EXPECT_EQ(via.entries.size(), 4U);
    EXPECT_FALSE(via.entries[0].protoName);
    EXPECT_EQ(via.entries[0].protoVersion, "1.0");
    EXPECT_EQ(via.entries[0].receivedBy, "ricky");
    EXPECT_TRUE(via.entries[0].comment.empty());
    EXPECT_FALSE(via.entries[1].protoName);
    EXPECT_EQ(via.entries[1].protoVersion, "1.1");
    EXPECT_EQ(via.entries[1].receivedBy, "ethel");
    EXPECT_TRUE(via.entries[1].comment.empty());
    EXPECT_FALSE(via.entries[2].protoName);
    EXPECT_EQ(via.entries[2].protoVersion, "1.1");
    EXPECT_EQ(via.entries[2].receivedBy, "fred");
    EXPECT_TRUE(via.entries[2].comment.empty());
    EXPECT_FALSE(via.entries[3].protoName);
    EXPECT_EQ(via.entries[3].protoVersion, "1.0");
    EXPECT_EQ(via.entries[3].receivedBy, "lucy");
    EXPECT_TRUE(via.entries[3].comment.empty());


    via.entries.clear();
    EXPECT_TRUE(via.parse("HTTP/1.0 ricky"));
    EXPECT_EQ(via.entries.size(), 1U);
    EXPECT_EQ(*via.entries[0].protoName, "HTTP");
    EXPECT_EQ(via.entries[0].protoVersion, "1.0");
    EXPECT_EQ(via.entries[0].receivedBy, "ricky");


    via.entries.clear();
    EXPECT_FALSE(via.parse("HTTP/1.0, test"));


    via.entries.clear();
    EXPECT_TRUE(via.parse(""));


    via.entries.clear();
    EXPECT_FALSE(via.parse("h"));


    via.entries.clear();
    EXPECT_TRUE(via.parse("h g"));
    EXPECT_EQ(via.entries.size(), 1U);
    EXPECT_FALSE(via.entries[0].protoName);
    EXPECT_EQ(via.entries[0].protoVersion, "h");
    EXPECT_EQ(via.entries[0].receivedBy, "g");
    EXPECT_TRUE(via.entries[0].comment.empty());


    via.entries.clear();
    EXPECT_TRUE(via.parse("  h  g   ,    h   z   mm , p/v  ps"));
    EXPECT_EQ(via.entries.size(), 3U);
    EXPECT_FALSE(via.entries[0].protoName);
    EXPECT_EQ(via.entries[0].protoVersion, "h");
    EXPECT_EQ(via.entries[0].receivedBy, "g");
    EXPECT_TRUE(via.entries[0].comment.empty());
    EXPECT_FALSE(via.entries[1].protoName);
    EXPECT_EQ(via.entries[1].protoVersion, "h");
    EXPECT_EQ(via.entries[1].receivedBy, "z");
    EXPECT_EQ(via.entries[1].comment, "mm ");
    EXPECT_EQ(*via.entries[2].protoName, "p");
    EXPECT_EQ(via.entries[2].protoVersion, "v");
    EXPECT_EQ(via.entries[2].receivedBy, "ps");
    EXPECT_TRUE(via.entries[2].comment.empty());


    via.entries.clear();
    EXPECT_TRUE(via.parse("1.0 fred, 1.1 nowhere.com (Apache/1.1) Commanch Whooyanch"));
    EXPECT_EQ(via.entries.size(), 2U);
    EXPECT_FALSE(via.entries[0].protoName);
    EXPECT_EQ(via.entries[0].protoVersion, "1.0");
    EXPECT_EQ(via.entries[0].receivedBy, "fred");
    EXPECT_TRUE(via.entries[0].comment.empty());
    EXPECT_EQ(via.entries[1].protoVersion, "1.1");
    EXPECT_EQ(via.entries[1].receivedBy, "nowhere.com");
    EXPECT_EQ(via.entries[1].comment, "(Apache/1.1) Commanch Whooyanch");


    via.entries.clear();
    EXPECT_TRUE(via.parse("1.1 {47bf37a0-72a6-2890-b967-5da9c390d28a}"));
    EXPECT_EQ(via.entries.size(), 1U);
    EXPECT_FALSE(via.entries[0].protoName);
    EXPECT_EQ(via.entries[0].protoVersion, "1.1");
    EXPECT_EQ(via.entries[0].receivedBy, "{47bf37a0-72a6-2890-b967-5da9c390d28a}");
    EXPECT_TRUE(via.entries[0].comment.empty());


    via.entries.clear();
    EXPECT_FALSE(via.parse(" "));
}

TEST(HttpTypesHeaderVia, toString)
{
    header::Via via;
    header::Via::ProxyEntry entry;
    entry.protoVersion = "1.0";
    entry.receivedBy = "{bla-bla-bla}";
    via.entries.push_back(entry);

    entry.protoName = "HTTP";
    entry.protoVersion = "1.0";
    entry.receivedBy = "{bla-bla-bla-bla}";
    via.entries.push_back(entry);

    entry.protoName = "HTTP";
    entry.protoVersion = "1.1";
    entry.receivedBy = "{test-test-test-test}";
    entry.comment = "qweasd123";
    via.entries.push_back(entry);

    EXPECT_EQ(
        via.toString(),
        "1.0 {bla-bla-bla}, HTTP/1.0 {bla-bla-bla-bla}, HTTP/1.1 {test-test-test-test} qweasd123");

    header::Via via2;
    EXPECT_TRUE(via2.parse(via.toString()));
    EXPECT_EQ(via, via2);
}

//-------------------------------------------------------------------------------------------------
// Accept-Encoding header tests.

TEST(HttpTypesHeaderAcceptEncoding, parse)
{
    {
        header::AcceptEncodingHeader acceptEncoding("gzip, deflate, sdch");
        ASSERT_TRUE(acceptEncoding.encodingIsAllowed("gzip"));
        ASSERT_TRUE(acceptEncoding.encodingIsAllowed("deflate"));
        ASSERT_TRUE(acceptEncoding.encodingIsAllowed("sdch"));
        ASSERT_TRUE(acceptEncoding.encodingIsAllowed("identity"));
        ASSERT_FALSE(acceptEncoding.encodingIsAllowed("qweasd123"));
    }

    {
        header::AcceptEncodingHeader acceptEncoding("gzip, deflate, sdch, identity;q=0");
        ASSERT_TRUE(acceptEncoding.encodingIsAllowed("gzip"));
        ASSERT_TRUE(acceptEncoding.encodingIsAllowed("deflate"));
        ASSERT_TRUE(acceptEncoding.encodingIsAllowed("sdch"));
        ASSERT_FALSE(acceptEncoding.encodingIsAllowed("identity"));
        ASSERT_FALSE(acceptEncoding.encodingIsAllowed("qweasd123"));
    }

    {
        header::AcceptEncodingHeader acceptEncoding("");
        ASSERT_FALSE(acceptEncoding.encodingIsAllowed("gzip"));
        ASSERT_TRUE(acceptEncoding.encodingIsAllowed("identity"));
    }

    {
        header::AcceptEncodingHeader acceptEncoding("gzip;q=0, deflate;q=0.5, sdch");
        ASSERT_FALSE(acceptEncoding.encodingIsAllowed("gzip"));
        ASSERT_TRUE(acceptEncoding.encodingIsAllowed("deflate"));
        ASSERT_TRUE(acceptEncoding.encodingIsAllowed("sdch"));
        ASSERT_TRUE(acceptEncoding.encodingIsAllowed("identity"));
    }

    {
        header::AcceptEncodingHeader acceptEncoding("gzip;q=0, deflate;q=0.5, sdch,*");
        ASSERT_FALSE(acceptEncoding.encodingIsAllowed("gzip"));
        ASSERT_TRUE(acceptEncoding.encodingIsAllowed("deflate"));
        ASSERT_TRUE(acceptEncoding.encodingIsAllowed("sdch"));
        ASSERT_TRUE(acceptEncoding.encodingIsAllowed("identity"));
        ASSERT_TRUE(acceptEncoding.encodingIsAllowed("qweasd123"));
    }

    {
        header::AcceptEncodingHeader acceptEncoding("gzip;q=0, deflate;q=0.5, sdch,*;q=0.0");
        ASSERT_FALSE(acceptEncoding.encodingIsAllowed("gzip"));
        ASSERT_TRUE(acceptEncoding.encodingIsAllowed("deflate"));
        ASSERT_TRUE(acceptEncoding.encodingIsAllowed("sdch"));
        ASSERT_FALSE(acceptEncoding.encodingIsAllowed("identity"));
        ASSERT_FALSE(acceptEncoding.encodingIsAllowed("qweasd123"));
    }

    {
        header::AcceptEncodingHeader acceptEncoding("gzip;q=1.0, identity;q=0.5, *;q=0");
        ASSERT_TRUE(acceptEncoding.encodingIsAllowed("gzip"));
        ASSERT_FALSE(acceptEncoding.encodingIsAllowed("deflate"));
        ASSERT_TRUE(acceptEncoding.encodingIsAllowed("identity"));
        ASSERT_FALSE(acceptEncoding.encodingIsAllowed("qweasd123"));
    }
}

//-------------------------------------------------------------------------------------------------
// WWWAuthenticate header tests.

TEST(HttpTypesHeaderWWWAuthenticate, parse)
{
    {
        static constexpr char testData[] =
            "Digest realm=\"AXIS_ACCC8E338EDF\", "
            "nonce=\"p65VeyEWBQA=0b,7e4955ab1d73d00a4b903c19d91c67931ef7ad\", "
            "algorithm=MD5, "
            "qop=\"auth\"";

        header::WWWAuthenticate auth;
        ASSERT_TRUE(auth.parse(testData));
        ASSERT_EQ(auth.authScheme, header::AuthScheme::digest);
        ASSERT_EQ(auth.params.size(), 4);
        ASSERT_EQ(auth.params["realm"], "AXIS_ACCC8E338EDF");
        ASSERT_EQ(auth.params["algorithm"], "MD5");
        ASSERT_EQ(auth.params["qop"], "auth");
        ASSERT_EQ(auth.params["nonce"], "p65VeyEWBQA=0b,7e4955ab1d73d00a4b903c19d91c67931ef7ad");
    }

    {
        static constexpr char testData[] =
            "Digest realm=AXIS_ACCC8E338EDF, "
            "nonce=\"p65VeyEWBQA=0b7e4955ab1d73d00a4b903c19d91c67931ef7ad\", "
            "algorithm=MD5, "
            "qop=auth";

        header::WWWAuthenticate auth;
        ASSERT_TRUE(auth.parse(testData));
        ASSERT_EQ(auth.authScheme, header::AuthScheme::digest);
        ASSERT_EQ(auth.params.size(), 4);
        ASSERT_EQ(auth.params["realm"], "AXIS_ACCC8E338EDF");
        ASSERT_EQ(auth.params["algorithm"], "MD5");
        ASSERT_EQ(auth.params["qop"], "auth");
        ASSERT_EQ(auth.params["nonce"], "p65VeyEWBQA=0b7e4955ab1d73d00a4b903c19d91c67931ef7ad");
    }

    {
        static constexpr char testData[] = "foo";

        header::WWWAuthenticate auth;
        ASSERT_FALSE(auth.parse(testData));
        ASSERT_EQ(auth.authScheme, header::AuthScheme::none);
        ASSERT_TRUE(auth.params.empty());
    }

    {
        static constexpr char testData[] = "";

        header::WWWAuthenticate auth;
        ASSERT_FALSE(auth.parse(testData));
        ASSERT_EQ(auth.authScheme, header::AuthScheme::none);
        ASSERT_TRUE(auth.params.empty());
    }

    {
        static constexpr char testData[] = " ";

        header::WWWAuthenticate auth;
        ASSERT_FALSE(auth.parse(testData));
        ASSERT_EQ(auth.authScheme, header::AuthScheme::none);
        ASSERT_TRUE(auth.params.empty());
    }

    {
        static constexpr char testData[] = "Basic";

        header::WWWAuthenticate auth;
        ASSERT_TRUE(auth.parse(testData));
        ASSERT_EQ(auth.authScheme, header::AuthScheme::basic);
        ASSERT_TRUE(auth.params.empty());
    }

    {
        static constexpr char testData[] = "Basic ";

        header::WWWAuthenticate auth;
        ASSERT_TRUE(auth.parse(testData));
        ASSERT_EQ(auth.authScheme, header::AuthScheme::basic);
        ASSERT_TRUE(auth.params.empty());
    }

    {
        static constexpr char testData[] = "Basic              ";

        header::WWWAuthenticate auth;
        ASSERT_TRUE(auth.parse(testData));
        ASSERT_EQ(auth.authScheme, header::AuthScheme::basic);
        ASSERT_TRUE(auth.params.empty());
    }

    // TODO: #akolesnikov Uncomment and fix.
    //{
    //    static const char testData[] =
    //        "Digest realm=AXIS_ACCC8E338EDF, "
    //        "nonce=p65VeyEWBQA=0b7e4955ab1d73d00a4b903c19d91c67931ef7ad, "
    //        "algorithm=MD5, qop=auth";

    //    header::WWWAuthenticate auth;
    //    ASSERT_FALSE(auth.parse(testData));
    //}
}

//-------------------------------------------------------------------------------------------------
// Authorization.

TEST(HttpTypesHeaderAuthorization, parse)
{
    {
        static constexpr char testData[] =
            "Digest username=\"Mufasa\","
            "realm=\"testrealm@host.com\","
            "nonce=\"dcd98b7102dd2f0e8b11d0f600bfb0c093\","
            "uri=\"/dir/index.html\","
            "qop=auth,"
            "nc=00000001,"
            "cnonce=\"0a4f113b\","
            "response=\"6629fae49393a05397450978507c4ef1\","
            "opaque=\"5ccc069c403ebaf9f0171e9517f40e41\"";
        header::Authorization auth;
        ASSERT_TRUE(auth.parse(testData));
        ASSERT_EQ(auth.authScheme, header::AuthScheme::digest);
        ASSERT_EQ(auth.userid(), "Mufasa");
        ASSERT_EQ(auth.digest->params["username"], "Mufasa");
        ASSERT_EQ(auth.digest->params["realm"], "testrealm@host.com");
        ASSERT_EQ(auth.digest->params["nonce"], "dcd98b7102dd2f0e8b11d0f600bfb0c093");
        ASSERT_EQ(auth.digest->params["uri"], "/dir/index.html");
        ASSERT_EQ(auth.digest->params["qop"], "auth");
        ASSERT_EQ(auth.digest->params["nc"], "00000001");
        ASSERT_EQ(auth.digest->params["cnonce"], "0a4f113b");
        ASSERT_EQ(auth.digest->params["response"], "6629fae49393a05397450978507c4ef1");
        ASSERT_EQ(auth.digest->params["opaque"], "5ccc069c403ebaf9f0171e9517f40e41");
    }

    {
        static constexpr char testData[] =
            "Digest realm=AXIS_ACCC8E338EDF, "
            "nonce=\"p65VeyEWBQA=0b7e4955ab1d73d00a4b903c19d91c67931ef7ad\", "
            "algorithm=MD5, "
            "qop=auth";

        header::Authorization auth;
        ASSERT_TRUE(auth.parse(testData));
        ASSERT_EQ(auth.authScheme, header::AuthScheme::digest);
        ASSERT_EQ(auth.digest->params.size(), 4);
        ASSERT_EQ(auth.digest->params["realm"], "AXIS_ACCC8E338EDF");
        ASSERT_EQ(auth.digest->params["algorithm"], "MD5");
        ASSERT_EQ(auth.digest->params["qop"], "auth");
        ASSERT_EQ(auth.digest->params["nonce"], "p65VeyEWBQA=0b7e4955ab1d73d00a4b903c19d91c67931ef7ad");
    }

    {
        static constexpr char testData[] = "Basic QWxhZGRpbjpvcGVuIHNlc2FtZQ==";

        header::Authorization auth;
        ASSERT_TRUE(auth.parse(testData));
        ASSERT_EQ(auth.authScheme, header::AuthScheme::basic);
        ASSERT_EQ(auth.basic->userid, "Aladdin");
        ASSERT_EQ(auth.basic->password, "open sesame");
    }

    {
        static constexpr char username[] = "johndoe@example.com";
        static constexpr char password[] = "pass!@#$%^&*()_-+=;:\\'\"`~,./|?[]{}";

        const auto basicHeaderValueStr =
            "Basic " + nx::utils::toBase64(username + std::string(":") + password);

        header::Authorization auth;
        ASSERT_TRUE(auth.parse(basicHeaderValueStr));
        ASSERT_EQ(auth.authScheme, header::AuthScheme::basic);
        ASSERT_EQ(username, auth.basic->userid);
        ASSERT_EQ(password, auth.basic->password);
    }
}

TEST(HttpTypesHeaderAuthorization, serialize)
{
    header::Authorization auth(header::AuthScheme::digest);
    auth.digest->params["realm"] = "AXIS_ACCC8E338EDF";
    auth.digest->params["algorithm"] = "MD5";
    auth.digest->params["qop"] = "auth";
    auth.digest->params["nonce"] = "p65VeyEWBQA=0b7e4955ab1d73d00a4b903c19d91c67931ef7ad";

    static constexpr char expected[] =
        "Digest realm=\"AXIS_ACCC8E338EDF\", "
        "nonce=\"p65VeyEWBQA=0b7e4955ab1d73d00a4b903c19d91c67931ef7ad\", "
        "algorithm=\"MD5\", "
        "qop=\"auth\"";

    ASSERT_EQ(expected, auth.serialized());
}

//-------------------------------------------------------------------------------------------------
// Keep-Alive.

TEST(HttpTypesHeaderKeepAlive, parse)
{
    {
        static constexpr char testData[] = "timeout=60, max=100";

        header::KeepAlive keepAlive;
        ASSERT_TRUE(keepAlive.parse(testData));
        ASSERT_EQ(std::chrono::seconds(60), keepAlive.timeout);
        ASSERT_TRUE(static_cast<bool>(keepAlive.max));
        ASSERT_EQ(100, *keepAlive.max);
    }

    {
        static constexpr char testData[] = "  timeout=60 ";

        header::KeepAlive keepAlive;
        ASSERT_TRUE(keepAlive.parse(testData));
        ASSERT_EQ(std::chrono::seconds(60), keepAlive.timeout);
        ASSERT_FALSE(static_cast<bool>(keepAlive.max));
    }

    {
        static const char testData[] = "  timeout=60  , max=100  ";

        header::KeepAlive keepAlive;
        ASSERT_TRUE(keepAlive.parse(testData));
        ASSERT_EQ(std::chrono::seconds(60), keepAlive.timeout);
        ASSERT_TRUE(static_cast<bool>(keepAlive.max));
        ASSERT_EQ(100, *keepAlive.max);
    }

    {
        static constexpr char testData[] = " max=100  ";

        header::KeepAlive keepAlive;
        ASSERT_FALSE(keepAlive.parse(testData));
    }
}

//-------------------------------------------------------------------------------------------------
// Server

class HttpTypesHeaderServer:
    public ::testing::Test
{
protected:
    void test(
        bool isValid,
        const header::Server& serverHeader,
        const std::string& serializedValue)
    {
        if (isValid)
            testSerialization(serverHeader, serializedValue);

        testParsing(isValid, serverHeader, serializedValue);
    }

    void testParsing(
        bool isValid,
        const header::Server& serverHeader,
        const std::string& serializedValue)
    {
        header::Server headerToParse;
        ASSERT_EQ(isValid, headerToParse.parse(serializedValue));
        if (isValid)
        {
            ASSERT_EQ(serverHeader, headerToParse);
        }
    }

    void testSerialization(
        const header::Server& serverHeader,
        const std::string& expectedSerializedValue)
    {
        const auto result = serverHeader.toString();
        ASSERT_TRUE(nx::utils::startsWith(result, expectedSerializedValue));
    }
};

TEST_F(HttpTypesHeaderServer, single_product_without_comment)
{
    header::Server serverHeader;
    serverHeader.products.clear();
    serverHeader.products.push_back(
        header::Server::Product{
            "ProductName", "1.2.3.4", "" });
    test(true, serverHeader, "ProductName/1.2.3.4");
}

TEST_F(HttpTypesHeaderServer, single_product_without_version)
{
    header::Server serverHeader;
    serverHeader.products.clear();
    serverHeader.products.push_back(
        header::Server::Product{"ProductName", "", ""});
    test(true, serverHeader, "ProductName");
}

TEST_F(HttpTypesHeaderServer, multiple_product_header_is_parsed)
{
    header::Server serverHeader;
    serverHeader.products.clear();
    serverHeader.products.push_back(
        header::Server::Product{"Product1", "1.2.3.4", "" });
    serverHeader.products.push_back(
        header::Server::Product{"Product2", "5.6.7.8", "comment2" });
    test(true, serverHeader, "Product1/1.2.3.4 Product2/5.6.7.8 (comment2)");
}

TEST_F(HttpTypesHeaderServer, empty_string_is_error)
{
    header::Server serverHeader;
    serverHeader.products.clear();
    test(false, serverHeader, "");
}

TEST_F(HttpTypesHeaderServer, empty_product_name_is_error)
{
    header::Server serverHeader;
    test(false, serverHeader, "/1.2.3.4");
}

TEST_F(HttpTypesHeaderServer, empty_product_name_is_error_2)
{
    header::Server serverHeader;
    test(false, serverHeader, "Product1/1.2.3.4 /5.6.7.8 (comment2)");
}

//-------------------------------------------------------------------------------------------------

TEST(HttpTypesHeaderHost, toString_omits_default_ports)
{
    ASSERT_EQ("example.com", header::Host("example.com:80").toString());
    ASSERT_EQ("example.com", header::Host("example.com:443").toString());
}

TEST(HttpTypesHeaderHost, toString_adds_non_default_ports)
{
    ASSERT_EQ("example.com:8080", header::Host("example.com:8080").toString());
    ASSERT_EQ("example.com:1443", header::Host("example.com:1443").toString());
}

//-------------------------------------------------------------------------------------------------

TEST(HttpTypesHeaderContentType, parse)
{
    {
        header::ContentType contentType("text/plain; charset=utf-8");
        ASSERT_EQ("text/plain", contentType.value);
        ASSERT_EQ("utf-8", contentType.charset);
        ASSERT_EQ("text/plain; charset=utf-8", contentType.toString());
    }

    {
        header::ContentType contentType("text/plain");
        ASSERT_EQ("text/plain", contentType.value);
        ASSERT_EQ("", contentType.charset);
        ASSERT_EQ("text/plain", contentType.toString());
    }
}

//-------------------------------------------------------------------------------------------------
// RequestLine.

TEST(HttpTypesRequestLine, parse)
{
    {
        RequestLine requestLine;
        ASSERT_TRUE(requestLine.parse(Buffer("GET /test/test/test?test=test&test HTTP/1.0")));
        ASSERT_EQ(requestLine.method, Method::get);
        ASSERT_EQ(requestLine.url, nx::utils::Url("/test/test/test?test=test&test"));
        ASSERT_EQ(requestLine.version, http_1_0);
    }

    {
        RequestLine requestLine;
        ASSERT_TRUE(requestLine.parse(Buffer("  PUT   /abc?def=ghi&jkl   HTTP/1.1")));
        ASSERT_EQ(requestLine.method, Method::put);
        ASSERT_EQ(requestLine.url, nx::utils::Url("/abc?def=ghi&jkl"));
        ASSERT_EQ(requestLine.version, http_1_1);
    }

    {
        RequestLine requestLine;
        ASSERT_FALSE(requestLine.parse(Buffer("GET    HTTP/1.1")));
        ASSERT_FALSE(requestLine.parse(Buffer()));
    }

    {
        RequestLine requestLine;
        ASSERT_TRUE(requestLine.parse(Buffer(
            "CONNECT 3219eff5-ac50-4c63-9f71-3c8a8f8f4385.c52c5aaf-b246-4465-8286-c10cf069a00b HTTP/1.1")));
        ASSERT_EQ(requestLine.method, Method::connect);
        ASSERT_EQ(requestLine.url.authority().toStdString(),
            "3219eff5-ac50-4c63-9f71-3c8a8f8f4385.c52c5aaf-b246-4465-8286-c10cf069a00b");
        ASSERT_EQ(requestLine.url.host().toStdString(),
            "3219eff5-ac50-4c63-9f71-3c8a8f8f4385.c52c5aaf-b246-4465-8286-c10cf069a00b");
        ASSERT_EQ(requestLine.version, http_1_1);
    }

    {
        RequestLine requestLine;
        ASSERT_TRUE(requestLine.parse(Buffer("CONNECT user@192.168.1.10:54321 HTTP/1.1")));
        ASSERT_EQ(requestLine.method, Method::connect);
        ASSERT_EQ(requestLine.url.authority().toStdString(), "user@192.168.1.10:54321");
        ASSERT_EQ(requestLine.url.userName().toStdString(), "user");
        ASSERT_EQ(requestLine.url.host().toStdString(), "192.168.1.10");
        ASSERT_EQ(requestLine.url.port(), 54321);
        ASSERT_EQ(requestLine.version, http_1_1);
    }
}

//-------------------------------------------------------------------------------------------------
// Request.

static constexpr char kTestRequest[] =
    "PLAY rtsp://192.168.0.25:7001/00-1A-07-0A-3A-88 RTSP/1.0\r\n"
    "CSeq: 2\r\n"
    "Range: npt=1423440107300000-1423682432925000\r\n"
    "Scale: 1\r\n"
    "x-guid: {ff69b2e0-1f1e-4512-8eb9-4d20b33587dc}\r\n"
    "Session: \r\n"
    "User-Agent: nx_network\r\n"
    "x-play-now: true\r\n"
    "x-media-step: 9693025000\r\n"
    "Authorization: Basic YWRtaW46MTIz\r\n"
    "x-server-guid: {d1a49afe-a2a2-990a-2c54-b77e768e6ad2}\r\n"
    "x-media-quality: low\r\n";

TEST(HttpTypesRequest, parse)
{
    Request request;
    ASSERT_TRUE(request.parse(kTestRequest));
    ASSERT_EQ(
        9693025000LL,
        nx::utils::stoll(getHeaderValue(request.headers, "x-media-step")));
}

TEST(HttpTypesRequest, removeCookieNoHeader)
{
    static constexpr char kTestRequest[] =
        "GET http://192.168.0.25:7001/ HTTP/1.0\r\n"
        "Accept-Language: en-us,en;q=0.5\r\n"
        "Accept-Encoding: gzip,deflate\t\n";

    Request request;
    ASSERT_TRUE(request.parse(kTestRequest));
    request.removeCookie("name");
    ASSERT_TRUE(getHeaderValue(request.headers, "Cookie").empty());
}

TEST(HttpTypesRequest, removeCookieSingleCookie)
{
    static constexpr char kTestRequest[] =
        "GET http://192.168.0.25:7001/ HTTP/1.0\r\n"
        "Accept-Language: en-us,en;q=0.5\r\n"
        "Cookie: x-runtime-guid={e0ede7dc-4add-4dfd-ba6c-3b660f206e15}\r\n"
        "Accept-Encoding: gzip,deflate\t\n";

    Request request;
    ASSERT_TRUE(request.parse(kTestRequest));
    request.removeCookie("x-runtime-guid");
    ASSERT_TRUE(getHeaderValue(request.headers, "Cookie").empty());
}

static constexpr char kTestRequestWithCookies[] =
    "GET http://192.168.0.25:7001/ HTTP/1.0\r\n"
    "Accept-Language: en-us,en;q=0.5\r\n"
    "Cookie: name1=value1; name2=value2; name3=value3\r\n"
    "Accept-Encoding: gzip,deflate\t\n";

TEST(HttpTypesRequest, removeCookieNoMatching)
{
    Request request;
    ASSERT_TRUE(request.parse(kTestRequestWithCookies));
    request.removeCookie("x-runtime-guid");
    ASSERT_EQ(
        "name1=value1; name2=value2; name3=value3",
        getHeaderValue(request.headers, "Cookie"));
}

TEST(HttpTypesRequest, removeCookieFirst)
{
    Request request;
    ASSERT_TRUE(request.parse(kTestRequestWithCookies));
    request.removeCookie("name1");
    ASSERT_EQ(
        "name2=value2; name3=value3",
        getHeaderValue(request.headers, "Cookie"));
}

TEST(HttpTypesRequest, removeCookieLast)
{
    Request request;
    ASSERT_TRUE(request.parse(kTestRequestWithCookies));
    request.removeCookie("name3");
    ASSERT_EQ(
        "name1=value1; name2=value2",
        getHeaderValue(request.headers, "Cookie"));
}

//-------------------------------------------------------------------------------------------------
// Response.

static constexpr char kTestResponse[] =
    "HTTP/1.1 200 OK\r\n"
    "Date: Wed, 06 Jun 2018 17:28:48 +0300\r\n"
    "Connection: Keep-Alive\r\n"
    "Keep-Alive: timeout=5\r\n"
    "X-Runtime-Guid: {d1a49afe-a2a2-990a-2c54-b77e768e6ad2}\r\n"
    "Set-Cookie: username=admin; Path=/\r\n"
    "Set-Cookie: authorizationKey=88d13fb76ea9; Path=/; HttpOnly\r\n"
    "Set-Cookie: orderId=_DELETED_COOKIE_VALUE_; Path=/; expires=Thu, 01 Jan 1970 00:00 : 00 GMT\r\n";

static const std::map<std::string, std::string> kTestCookie{
    {"username", "admin"},
    {"authorizationKey", "88d13fb76ea9"},
};

TEST(HttpTypesResponse, parse)
{
    Response request;
    ASSERT_TRUE(request.parse(kTestResponse));
    ASSERT_EQ(kTestCookie, request.getCookies());
    ASSERT_EQ("{d1a49afe-a2a2-990a-2c54-b77e768e6ad2}",
        getHeaderValue(request.headers, "X-Runtime-Guid"));

}

} // namespace nx::network::http::header
