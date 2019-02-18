#include <gtest/gtest.h>

#include <nx/network/url/url_builder.h>

namespace nx::network::url {
namespace test {

namespace {

static const nx::utils::Url kSampleUrl("https://localhost:7001");

} // namespace

class UrlBuilderTest: public ::testing::Test
{
};

class BuildUrlFromHost:
    public UrlBuilderTest,
    public ::testing::WithParamInterface<std::string>
{
};

TEST_P(BuildUrlFromHost, urlIsValid)
{
    const std::string hostname = GetParam();
    const SocketAddress address(hostname, 7001);
    const auto url = Builder()
        .setScheme(nx::network::http::kSecureUrlSchemeName)
        .setEndpoint(address)
        .toUrl();
    ASSERT_TRUE(url.isValid());
    ASSERT_TRUE(url.toQUrl().isValid());
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

INSTANTIATE_TEST_CASE_P(UrlBuilderParseIPv6Host,
	BuildUrlFromHost,
	::testing::ValuesIn(kIPv6HostNames));

class BuildUrlWithPath:
    public UrlBuilderTest,
    public ::testing::WithParamInterface<std::string>
{
};

TEST_P(BuildUrlWithPath, urlIsValid)
{
    const std::string path = GetParam();
    const auto url = Builder(kSampleUrl)
        .setPath(path)
        .toUrl();
    ASSERT_TRUE(url.isValid());
    ASSERT_TRUE(url.toQUrl().isValid());
}

TEST_P(BuildUrlWithPath, pathStartsFromSingleSlash)
{
    const std::string path = GetParam();
    const auto url = Builder(kSampleUrl)
        .setPath(path)
        .toUrl();
    ASSERT_EQ(url.path().isEmpty(), path.empty());
    ASSERT_TRUE(url.path().isEmpty() || url.path().startsWith("/"));
}

TEST_P(BuildUrlWithPath, noDoubleSlashInPath)
{
    const std::string path = GetParam();
    const auto url = Builder(kSampleUrl)
        .setPath(path)
        .toUrl();
    ASSERT_FALSE(url.path().contains("//"));
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

INSTANTIATE_TEST_CASE_P(UrlBuilderParsePath,
	BuildUrlWithPath,
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

TEST_P(AppendPathToUrl, appendedPathIsValid)
{
    const UrlPathParts parts = GetParam();
    Builder builder = Builder(kSampleUrl)
        .setPath(parts.path);

    for (const auto& part: parts.parts)
        builder.appendPath(part);

    const auto url = builder.toUrl();
    ASSERT_TRUE(url.isValid());
    ASSERT_TRUE(url.toQUrl().isValid());
}

TEST_P(AppendPathToUrl, appendedPathIsExpected)
{
    const UrlPathParts parts = GetParam();
    Builder builder = Builder(kSampleUrl)
        .setPath(parts.path);

    for (const auto& part: parts.parts)
        builder.appendPath(part);

    const auto url = builder.toUrl();
    ASSERT_EQ(url.path().toStdString(), parts.expectedPath);
}

static std::vector<UrlPathParts> kPathParts
{
	{"", {}, ""},
    {"path", {}, "/path"},
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

INSTANTIATE_TEST_CASE_P(UrlBuilderAppendPath,
	AppendPathToUrl,
	::testing::ValuesIn(kPathParts));

} // namespace test
} // namespace nx::network::url


