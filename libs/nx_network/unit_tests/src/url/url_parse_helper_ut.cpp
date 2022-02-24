// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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
    ASSERT_EQ("/sample/", normalizePath(std::string("//sample//")));

    ASSERT_EQ("/sample/url/path/", normalizePath(std::string("/./sample/./url/path/")));
    ASSERT_EQ("/url/path/", normalizePath(std::string("/sample/../url/path/")));
    ASSERT_EQ("/../url/path", normalizePath(std::string("/sample/../../url/path")));
    ASSERT_EQ("../path/", normalizePath(std::string("../url/../path/")));
}

} // namespace test
} // namespace url
} // namespace network
} // namespace nx
