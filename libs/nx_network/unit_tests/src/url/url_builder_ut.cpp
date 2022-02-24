// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/network/url/url_builder.h>
#include <nx/network/http/http_types.h>

namespace nx::network::url::test {

namespace {

static const nx::utils::Url kSampleUrl("https://localhost:7001");
static const std::string kSampleHost = "localhost";

static bool urlIsValid(const nx::utils::Url& url)
{
    return url.isValid() && url.toQUrl().isValid();
}

} // namespace

class UrlBuilderTest:
    public ::testing::Test
{
};

class BuildUrlWithPath:
    public UrlBuilderTest,
    public ::testing::WithParamInterface<std::string>
{
protected:
	std::string path() const { return GetParam(); }
};

class BuildUrlFromHost:
    public UrlBuilderTest,
    public ::testing::WithParamInterface<std::string>
{
protected:
    std::string hostname() const { return GetParam(); }
};

TEST_P(BuildUrlFromHost, urlIsValid)
{
    const SocketAddress address(hostname(), 7001);
    const auto url = Builder()
        .setScheme(nx::network::http::kSecureUrlSchemeName)
        .setEndpoint(address)
        .toUrl();
    ASSERT_TRUE(urlIsValid(url));
    ASSERT_FALSE(url.host().isEmpty());
}

static std::vector<std::string> kIPv6HostNames{
    // Full notation.
    "2001:0db8:11a3:09d7:1f34:8a2e:07a0:765d",
    "[2001:0db8:11a3:09d7:1f34:8a2e:07a0:765d]",
    // Shortened notation.
    "[2001::9d38:6ab8:384e:1c3e:aaa2]",
    "2001::9d38:6ab8:384e:1c3e:aaa2",
    "::",
    "::1",
    "2001::",
    // Zeroes in the notation.
    "[2001:0000:11a3:09d7:1f34:8a2e:07a0:765d]",
    "2001:0000:11a3:09d7:1f34:8a2e:07a0:765d",
    // Single zero in the notation.
    "[2001:0:11a3:09d7:1f34:8a2e:07a0:765d]",
    "2001:0:11a3:09d7:1f34:8a2e:07a0:765d"
};

INSTANTIATE_TEST_SUITE_P(UrlBuilderParseIPv6Host,
	BuildUrlFromHost,
	::testing::ValuesIn(kIPv6HostNames));

class BuildUrlWithHostAndPath: public BuildUrlWithPath
{
protected:
    nx::utils::Url url() const { return Builder(kSampleUrl).setPath(path()); }
};

TEST_P(BuildUrlWithHostAndPath, urlIsValid)
{
    ASSERT_TRUE(urlIsValid(url()));
}

TEST_P(BuildUrlWithHostAndPath, urlIsValidWhenHostIsSetLater)
{
    const auto url = Builder()
        .setPath(path())
        .setHost(kSampleHost)
        .toUrl();
    ASSERT_TRUE(urlIsValid(url));
}

TEST_P(BuildUrlWithHostAndPath, urlIsValidWhenWhenEndpointIsSetLater)
{
    const auto url = Builder()
        .setPath(path())
        .setEndpoint(SocketAddress(kSampleHost))
        .toUrl();
    ASSERT_TRUE(urlIsValid(url));
}

TEST_P(BuildUrlWithHostAndPath, pathStartsFromSingleSlash)
{
    const auto url = this->url();
    ASSERT_EQ(url.path().isEmpty(), path().empty());
    ASSERT_TRUE(url.path().isEmpty() || url.path().startsWith("/"));
}

TEST_P(BuildUrlWithHostAndPath, pathStartsFromSingleSlashWhenHostIsSetLater)
{
    const auto url = Builder()
        .setPath(path())
        .setHost(kSampleHost)
        .toUrl();
    ASSERT_EQ(url.path().isEmpty(), path().empty());
    ASSERT_TRUE(url.path().isEmpty() || url.path().startsWith("/"));
}

TEST_P(BuildUrlWithHostAndPath, pathStartsFromSingleSlashWhenEndpointIsSetLater)
{
    const auto url = Builder()
        .setPath(path())
        .setEndpoint(SocketAddress(kSampleHost))
        .toUrl();
    ASSERT_EQ(url.path().isEmpty(), path().empty());
    ASSERT_TRUE(url.path().isEmpty() || url.path().startsWith("/"));
}

TEST_P(BuildUrlWithHostAndPath, noDoubleSlashInPath)
{
    ASSERT_FALSE(url().path().contains("//"));
}

class BuildUrlWithPathOnly: public BuildUrlWithPath
{
protected:
    nx::utils::Url url() const { return Builder().setPath(path()); }
};

TEST_P(BuildUrlWithPathOnly, urlIsValid)
{
    const auto url = this->url();
    ASSERT_TRUE(path().empty() || url.isValid());
    ASSERT_TRUE(path().empty() || url.toQUrl().isValid());
}

TEST_P(BuildUrlWithPathOnly, pathStartsFromNoSlash)
{
    const auto url = this->url();
    ASSERT_EQ(url.path().isEmpty(), path().empty());
    ASSERT_FALSE(url.path().startsWith("/"));
}

TEST_P(BuildUrlWithPathOnly, noDoubleSlashInPath)
{
    ASSERT_FALSE(url().path().contains("//"));
}

static std::vector<std::string> kPaths {
    std::string(),
    "path",
    "/path",
    "//path",
    "///path",
    "path/",
    "/path/",
    "//path/",
    "///path/",
    "path//",
    "/path//",
    "//path//",
    "///path//",
    "/path/long/",
    "//path//long//",
    "///path//long",
    "///path///long///",
};

INSTANTIATE_TEST_SUITE_P(UrlBuilderParsePath,
	BuildUrlWithHostAndPath,
	::testing::ValuesIn(kPaths));

INSTANTIATE_TEST_SUITE_P(UrlBuilderParsePath,
	BuildUrlWithPathOnly,
	::testing::ValuesIn(kPaths));

struct UrlPathParts
{
    std::string path;
    std::vector<std::string> parts;
    std::string expectedPath;

    UrlPathParts(
        const std::string& path,
		const std::vector<std::string>& parts,
		const std::string& expectedPath)
        :
        path(path),
        parts(parts),
        expectedPath(expectedPath)
    {}
};

void PrintTo(UrlPathParts value, ::std::ostream* os)
{
    *os
        << "( " << value.path << ", [";
    for (const auto& part: value.parts)
        *os << part << ", ";
    *os <<"], " << value.expectedPath << ")";
}

class AppendPathToUrl:
    public UrlBuilderTest,
    public ::testing::WithParamInterface<UrlPathParts>
{
};

TEST_P(AppendPathToUrl, appended)
{
    const UrlPathParts parts = GetParam();
    Builder builder = Builder(kSampleUrl)
        .setPath(parts.path);

    for (const auto& part: parts.parts)
        builder.appendPath(part);

    const auto url = builder.toUrl();
    EXPECT_TRUE(urlIsValid(url));
    EXPECT_EQ(url.path().toStdString(), parts.expectedPath);
}

TEST_P(AppendPathToUrl, appendedParts)
{
    const UrlPathParts parts = GetParam();
    Builder builder = Builder(kSampleUrl)
        .setPath(parts.path);

    switch (parts.parts.size())
    {
        case 0: break;
        case 1: builder.appendPathParts(parts.parts[0]); break;
        case 2: builder.appendPathParts(parts.parts[0], parts.parts[1]); break;
        default: FAIL() << "Unsupported number of arguments"; break;
    };

    const auto url = builder.toUrl();
    EXPECT_TRUE(urlIsValid(url));
    EXPECT_EQ(url.path().toStdString(), parts.expectedPath);
}

TEST_P(AppendPathToUrl, setParts)
{
    const UrlPathParts parts = GetParam();
    Builder builder = Builder(kSampleUrl);

    switch (parts.parts.size())
    {
        case 0: builder.setPath(parts.path); break;
        case 1: builder.setPathParts(parts.path, parts.parts[0]); break;
        case 2: builder.setPathParts(parts.path, parts.parts[0], parts.parts[1]); break;
        default: FAIL() << "Unsupported number of arguments"; break;
    };

    const auto url = builder.toUrl();
    EXPECT_TRUE(urlIsValid(url));
    EXPECT_EQ(url.path().toStdString(), parts.expectedPath);
}

static std::vector<UrlPathParts> kPathParts
{
	{"", {}, ""},
    {"", {""}, ""},
    {"path", {}, "/path"},
    {"path", {""}, "/path"},
    {"path", {"/", "/"}, "/path/"},
    {"path", {"part"}, "/path/part"},
    {"path", {"part1", "part2"}, "/path/part1/part2"},
    {"path/", {"/part"}, "/path/part"},
    {"path/", {"//part"}, "/path/part"},
    {"path//", {"part"}, "/path/part"},
    {"path//", {"/part"}, "/path/part"},
    {"path//", {"//part"}, "/path/part"},
    {"path///", {"///part///"}, "/path/part/"},
    {"path", {"part1/", "part2"}, "/path/part1/part2"},
    {"path", {"part1/", "/part2"}, "/path/part1/part2"},
    {"path", {"part1//", "part2"}, "/path/part1/part2"},
    {"path", {"part1//", "//part2"}, "/path/part1/part2"},
    {"path///", {"///part1///", "///part2///"}, "/path/part1/part2/"}
};

INSTANTIATE_TEST_SUITE_P(UrlBuilderAppendPath,
	AppendPathToUrl,
	::testing::ValuesIn(kPathParts));

class UrlBuilderQuery: public UrlBuilderTest
{
protected:
    static void expectQuery(const Builder& builder, const QString& expectedQuery)
    {
        const auto url = builder.toUrl();
        EXPECT_TRUE(urlIsValid(url));
        EXPECT_EQ(url.query(), expectedQuery);

        const auto querySuffix = expectedQuery.isEmpty() ? QString() : ("?" + expectedQuery);
        EXPECT_EQ(url.toString(), kSampleUrl.toString() + querySuffix);
    }
};

TEST_F(UrlBuilderQuery, addItem)
{
    Builder builder = Builder(kSampleUrl);
    expectQuery(builder, "");

    builder.addQueryItem("string", "stringValue");
    expectQuery(builder, "string=stringValue");

    builder.addQueryItem("number", 42);
    expectQuery(builder, "string=stringValue&number=42");

    builder.addQueryItem("status", (int) http::StatusCode::notFound);
    expectQuery(builder, "string=stringValue&number=42&status=404");
}

TEST_F(UrlBuilderQuery, escapedQuery)
{
    Builder builder = Builder(kSampleUrl);
    expectQuery(builder, "");

    builder.setQuery("id={<>}");
    ASSERT_EQ(builder.toUrl().query(), "id={<>}");

    builder.setQuery("id=%7B%3C%3E%7D");
    ASSERT_EQ(builder.toUrl().query(), "id={<>}");

    QUrl url;
    url.setQuery("id={<>}");
    builder.setQuery(url.query());
    ASSERT_EQ(builder.toUrl().query(), "id={<>}");

    url.setQuery("id=%7B%3C%3E%7D");
    builder.setQuery(url.query());
    ASSERT_EQ(builder.toUrl().query(), "id={<>}");
}

//-------------------------------------------------------------------------------------------------

class BuildUrlIpv6: public UrlBuilderTest {};

TEST_F(BuildUrlIpv6, host_with_scope)
{
    const auto url = Builder().setScheme("http")
        .setEndpoint(SocketAddress("fe80::f80b:5eab:64a8:7b63%14", 7001))
        .setPath("/foo").toUrl();

    ASSERT_TRUE(url.isValid());
    ASSERT_EQ("http", url.scheme());
    ASSERT_EQ("fe80::f80b:5eab:64a8:7b63%14", url.host());
    ASSERT_EQ(7001, url.port());
    ASSERT_EQ("/foo", url.path());
}

} // namespace nx::network::url::test
