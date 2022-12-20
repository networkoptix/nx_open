// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/utils/url_query.h>

namespace nx::utils::test {

class UrlQuery:
    public ::testing::Test
{
protected:
    template<typename T>
    void assertRepresentedAs(T val, const std::string& expectedStr)
    {
        auto keys = std::make_tuple(
            std::string("key"), std::string_view("key"), QString("key"), "key");

        // Testing with different key types.
        std::apply(
            [this, val, expectedStr](auto&&... args)
            {
                (this->test(args, val, expectedStr), ...);
            },
            keys);
    }

    template<typename T, typename Key>
    void test(Key key, T val, const std::string& expectedStr)
    {
        nx::utils::UrlQuery query;
        query.addQueryItem(key, val);

        ASSERT_EQ(val, query.queryItemValue<T>(key));
        ASSERT_EQ(expectedStr, query.queryItemValue<std::string>(key));
    }

    void assertBoolValueParsedAs(
        const std::string& query,
        const std::vector<std::pair<std::string, bool>>& expected)
    {
        nx::utils::UrlQuery q(QString::fromStdString(query));
        for (auto& item: expected)
        {
            ASSERT_EQ(item.second, q.queryItemValue<bool>(item.first));
        }
    }
};

TEST_F(UrlQuery, simple_types_representation)
{
    assertRepresentedAs(true, "1");
    assertRepresentedAs(false, "0");

    assertRepresentedAs(123, "123");
    assertRepresentedAs(123.34, "123.34");

    assertRepresentedAs(std::string("Hello, world!"), "Hello, world!");
    assertRepresentedAs(QString("Hello, world!"), "Hello, world!");
}

TEST_F(UrlQuery, different_bool_encodings_are_parsed_correctly)
{
    assertBoolValueParsedAs("a=1&b=0", {{"a", true}, {"b", false}});
    assertBoolValueParsedAs("a=true&b=false", {{"a", true}, {"b", false}});
    assertBoolValueParsedAs("a=TRUE&b=fAlSe", {{"a", true}, {"b", false}});
    assertBoolValueParsedAs("a&b", {{"a", true}, {"b", true}});
    assertBoolValueParsedAs("a", {{"a", true}, {"b", false}});
}

} // namespace nx::utils::test
