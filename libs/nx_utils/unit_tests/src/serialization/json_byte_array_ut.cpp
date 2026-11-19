// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/reflect/json/utils.h>
#include <nx/utils/json/byte_array.h>

namespace nx::json::test {

TEST(JsonByteArray, toByteArray)
{
    rapidjson::Document document;
    document.Parse(R"json({
        "escaped": "quote \" backslash \\ tab \t nul \u0000 end",
        "unicode": "\u0442\u0435\u0441\u0442 \ud83d\ude00",
        "values": [1, -2.5, 1e300, true, false, null, {}, []]
    })json");
    ASSERT_FALSE(document.HasParseError());

    const rapidjson::Value& value = document;
    ASSERT_EQ(toByteArray(value).toStdString(),
        nx::reflect::json_detail::getStringRepresentation(value));

    ASSERT_EQ(toByteArray(rapidjson::Value("text")), "\"text\"");
    ASSERT_EQ(toByteArray(rapidjson::Value(rapidjson::kObjectType)), "{}");
}

TEST(JsonByteArray, largeOutput)
{
    // Longer than the 4 KB step the stream grows the buffer by.
    const std::string text(10'000, 'x');
    const rapidjson::Value value(
        rapidjson::StringRef(text.data(), (rapidjson::SizeType) text.size()));
    ASSERT_EQ(toByteArray(value).toStdString(), '"' + text + '"');
}

} // namespace nx::json::test
