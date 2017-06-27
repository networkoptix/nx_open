#include <gtest/gtest.h>

#include <nx/network/url/url_parse_helper.h>

namespace nx {
namespace network {
namespace url {
namespace test {

TEST(Url, normalizePath)
{
    ASSERT_EQ("/sample/url/path", normalizePath("/sample/url/path"));
    ASSERT_EQ("/sample/url/path/", normalizePath("/sample/url/path/"));
    ASSERT_EQ("sampleUrlPath", normalizePath("sampleUrlPath"));

    ASSERT_EQ("/sample/url/path", normalizePath("//sample//url/path"));
    
    // TODO: #ak
    //ASSERT_EQ("/sample/url/path", normalizePath("///sample//url/path"));
}

} // namespace test
} // namespace url
} // namespace network
} // namespace nx
