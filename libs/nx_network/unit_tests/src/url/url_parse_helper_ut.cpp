#include <gtest/gtest.h>

#include <nx/network/url/url_parse_helper.h>

namespace nx {
namespace network {
namespace url {
namespace test {

TEST(Url, normalizePath)
{
    ASSERT_EQ("/sample/url/path", normalizePath(std::string("/sample/url/path")));
    ASSERT_EQ("/sample/url/path/", normalizePath(std::string("/sample/url/path/")));
    ASSERT_EQ("sampleUrlPath", normalizePath(std::string("sampleUrlPath")));
    ASSERT_EQ("/sample/url/path", normalizePath(std::string("//sample//url/path")));
    ASSERT_EQ("/sample/url/path", normalizePath(std::string("///sample//url/path")));
    ASSERT_EQ("/sample/url/path/", normalizePath(std::string("/sample///url/path////")));
}

} // namespace test
} // namespace url
} // namespace network
} // namespace nx
