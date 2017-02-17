#include <gtest/gtest.h>
#include <url.h>

using namespace utils;

TEST(UrlTest, CorrectUrl)
{
    Url url("test://some.host/some/path?key1=value1&key2=value2");

    ASSERT_TRUE(url.valid());
    ASSERT_EQ(url.scheme(), "test");
    ASSERT_EQ(url.url(), "test://some.host/some/path");
    ASSERT_EQ(url.params().size(), 2);
    ASSERT_EQ(url.params()["key1"], "value1");
    ASSERT_EQ(url.params()["key2"], "value2");
}

TEST(UrlTest, NoScheme)
{
    Url url("://some.host/some/path?key1=value1&key2=value2");
    ASSERT_FALSE(url.valid());

    Url url1("//some.host/some/path?key1=value1&key2=value2");
    ASSERT_FALSE(url1.valid());
}

TEST(UrlTest, WrongSchemeHostSeparator)
{
    Url url("test:/some.host/some/path?key1=value1&key2=value2");
    ASSERT_FALSE(url.valid());

    Url url1("test//some.host/some/path?key1=value1&key2=value2");
    ASSERT_FALSE(url1.valid());

    Url url2("test.some.host/some/path?key1=value1&key2=value2");
    ASSERT_FALSE(url2.valid());
}

TEST(UrlTest, NullHost)
{
    Url url("test:///adfa?key1=value1&key2=value2");
    ASSERT_FALSE(url.valid());

    Url url1("test://?key1=value1&key2=value2");
    ASSERT_FALSE(url1.valid());
}

TEST(UrlTest, NullKey)
{
    Url url("test://some.host/some/path?=value1&key2=value2");
    ASSERT_FALSE(url.valid());

    Url url1("test://some.host/some/path?key1=value1&=value2");
    ASSERT_FALSE(url1.valid());
}

TEST(UrlTest, NullValue)
{
    Url url("test://some.host/some/path?key1=&key2=value2");
    ASSERT_TRUE(url.valid());
    ASSERT_EQ(url.params()["key1"], "");
    ASSERT_EQ(url.params()["key2"], "value2");

    Url url1("test://some.host/some/path?key1=value1&key2=");
    ASSERT_TRUE(url1.valid());
    ASSERT_EQ(url1.params()["key1"], "value1");
    ASSERT_EQ(url1.params()["key2"], "");
}