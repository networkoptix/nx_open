// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <functional>
#include <vector>

#include <gtest/gtest.h>

#include <array>
#include <cstring>
#include <list>
#include <array>
#include <set>

#include <QtCore/QList>
#include <QtCore/QVector>

#include <nx/utils/string.h>

namespace nx::utils::test {

TEST(replaceStrings, emptySubstitutionsLeaveIntact)
{
    ASSERT_EQ(replaceStrings(
        "abc",
        {}),
        QString("abc"));
}

TEST(replaceStrings, noMatch)
{
    ASSERT_EQ(replaceStrings(
        "def",
        {{"a", "b"}}),
        QString("def"));
}

TEST(replaceStrings, independentSubstitutions)
{
    ASSERT_EQ(replaceStrings(
        "abc",
        {{"a", "d"}, {"c", "e"}}),
        QString("dbe"));
}

TEST(replaceStrings, sequentiveSubstitutions)
{
    ASSERT_EQ(replaceStrings(
        "abc",
        {{"a", "b"}, {"b", "c"}, {"c", "a"}}),
        QString("bca"));
}

TEST(replaceStrings, sequentiveSubstitutionsOfDifferentSize)
{
    ASSERT_EQ(replaceStrings(
        "abc",
        {{"a", "abc"}, {"b", "abc"}, {"c", "abc"}}),
        QString("abcabcabc"));
}

TEST(replaceStrings, sequentiveSubstitutionsWithOverriding)
{
    ASSERT_EQ(replaceStrings(
        "abc",
        {{"ab", "b"}, {"b", "c"}}),
        QString("bc"));
}

TEST(replaceStrings, substitutionStartsWithSubstringOfAnotherSubstitution)
{
    ASSERT_EQ(replaceStrings(
        "abac",
        {{"ab", "b"}, {"a", "c"}}),
        QString("bcc"));
}

TEST(replaceStrings, orderDoesMatter)
{
    ASSERT_EQ(replaceStrings(
        "abc",
        {{"a", "b"}, {"a", "c"}}),
        QString("bbc"));
}

TEST(replaceStrings, specialCharacters)
{
    ASSERT_EQ(replaceStrings(
        "|.*",
        {{".", "*"}, {"|", "*"}}),
        QString("***"));
}

TEST(replaceCharacters, nonFileNameCharacters)
{
    ASSERT_EQ(
        replaceNonFileNameCharacters("urn:uuid:1ec4ec50-1dd2-11b2-80f3-86228563f76f", '_'),
        "urn_uuid_1ec4ec50-1dd2-11b2-80f3-86228563f76f");
}

//-------------------------------------------------------------------------------------------------
// bytes <-> string

TEST(StringAndBytes, stringToBytes)
{
    ASSERT_EQ(123456, stringToBytes("123456"));
    ASSERT_EQ(123 * 1024, stringToBytes("123K"));
    ASSERT_EQ(123 * 1024, stringToBytes("123k"));

    ASSERT_EQ(123ULL << 20, stringToBytes("123m"));
    ASSERT_EQ(123ULL << 30, stringToBytes("123g"));
    ASSERT_EQ(123ULL << 40, stringToBytes("123t"));

    bool isOk = false;
    stringToBytes("123456", &isOk);
    ASSERT_TRUE(isOk);

    stringToBytes("aaa", &isOk);
    ASSERT_FALSE(isOk);

    stringToBytes("", &isOk);
    ASSERT_FALSE(isOk);
}

TEST(StringAndBytes, bytesToString)
{
    ASSERT_EQ("1K", bytesToString(1ULL << 10));
    ASSERT_EQ("1M", bytesToString(1ULL << 20));
    ASSERT_EQ("1G", bytesToString(1ULL << 30));
    ASSERT_EQ("1T", bytesToString(1ULL << 40));

    ASSERT_EQ("512G", bytesToString((1ULL << 40) / 2));
    ASSERT_EQ("1.5T", bytesToString((1ULL << 40) * 3 / 2, 2));
}

TEST(String, trimAndUnquote)
{
    ASSERT_EQ("foo", nx::utils::trimAndUnquote(QString("\"foo\"")));
    ASSERT_EQ("foo", nx::utils::trimAndUnquote(QString("foo")));
    ASSERT_EQ("foo", nx::utils::trimAndUnquote(QString("\"foo")));
    ASSERT_EQ("foo", nx::utils::trimAndUnquote(QString("foo\"")));

    ASSERT_EQ("", nx::utils::trimAndUnquote(QString()));
    ASSERT_EQ("", nx::utils::trimAndUnquote(QString("\"\"")));
    ASSERT_EQ("", nx::utils::trimAndUnquote(QString("\"")));
}

//-------------------------------------------------------------------------------------------------

TEST(String, strJoin)
{
    std::vector<QString> source1{"s1", "s2", "s3"};
    ASSERT_EQ(nx::utils::strJoin(source1, ", "), QString("s1, s2, s3"));
    ASSERT_EQ(nx::utils::strJoin(source1.begin() + 1, source1.end(), ", "), QString("s2, s3"));

    std::list<QByteArray>source2{"s1", "s2", "s3"};
    ASSERT_EQ(nx::utils::strJoin(source2, QByteArray()), QByteArray("s1s2s3"));

    std::set<std::string> source3{"s2", "s3", "s1"};
    ASSERT_EQ(nx::utils::strJoin(source3, "   "), std::string("s1   s2   s3"));
    ASSERT_EQ(nx::utils::strJoin(source3.rbegin(), source3.rend(), " "), std::string("s3 s2 s1"));

    std::array<QString, 3> source4{"s1", "s2", "s3"};
    ASSERT_EQ(nx::utils::strJoin(source4, QString("-")), QString("s1-s2-s3"));

    QVector<QString> source5{"s1"};
    ASSERT_EQ(nx::utils::strJoin(source5, ' '), QString("s1"));

    std::vector<std::string> source6{"s1", "", "s2", "", ""};
    ASSERT_EQ(nx::utils::strJoin(source6, std::string(",")), std::string("s1,,s2,,"));

    QList<std::string> source7;
    ASSERT_EQ(nx::utils::join(source7, std::string()), std::string());
}

//-------------------------------------------------------------------------------------------------

TEST(String, truncateToNul)
{
    QString testQString1(10, QChar(0));
    const char16_t testWString[] = u"abcdef";

    memcpy(testQString1.data(), testWString, sizeof(testWString));
    ASSERT_EQ(testQString1.size(), 10);
    ASSERT_NE(testQString1, QString("abcdef"));

    nx::utils::truncateToNul(&testQString1);
    ASSERT_EQ(testQString1.size(), 6);
    ASSERT_EQ(testQString1, QString("abcdef"));

    QString testQString2;
    nx::utils::truncateToNul(&testQString2);
    ASSERT_EQ(testQString2.size(), 0);

    QString testQString3(10, QChar(0));
    nx::utils::truncateToNul(&testQString3);
    ASSERT_EQ(testQString3.size(), 0);

    QString testQString4("abcdef");
    nx::utils::truncateToNul(&testQString4);
    ASSERT_EQ(testQString1.size(), 6);
    ASSERT_EQ(testQString1, QString("abcdef"));
}

//-------------------------------------------------------------------------------------------------

struct GenerateUniqueStringInput
{
    QStringList usedStrings;
    QString defaultString;
    QString templateString;
    QString expectedString;
};

class GenerateUniqueString:
    public ::testing::Test,
    public ::testing::WithParamInterface<GenerateUniqueStringInput>
{
};

TEST_P(GenerateUniqueString, validateExpectedString)
{
    const GenerateUniqueStringInput input = GetParam();
    EXPECT_EQ(
        generateUniqueString(input.usedStrings, input.defaultString, input.templateString),
        input.expectedString);
}

static std::vector<GenerateUniqueStringInput> kGenerateUniqueStringInput {
    {
        {},
        "New Layout",
        "New Layout %1",
        "New Layout"
    },
    {
        {{"New Layout"}},
        "New Layout",
        "New Layout %1",
        "New Layout 2"
    },
    {
        {{"New Layout 20"}},
        "New Layout",
        "New Layout %1",
        "New Layout 21"
    },
    {
        {},
        "New Layout (copy)",
        "New Layout (copy %2)",
        "New Layout (copy)"
    },
    {
        {{"New Layout (copy)"}},
        "New Layout (copy)",
        "New Layout (copy %2)",
        "New Layout (copy 2)"
    },
    {
        {{"New Layout (copy 20)"}},
        "New Layout (copy)",
        "New Layout (copy %2)",
        "New Layout (copy 21)"
    },
};

INSTANTIATE_TEST_SUITE_P(String, GenerateUniqueString,
    ::testing::ValuesIn(kGenerateUniqueStringInput));

//-------------------------------------------------------------------------------------------------

struct JsonFormatInput
{
    QString unformattedJson;
    QString formattedJson;
};

class JsonFormatting:
    public ::testing::Test,
    public ::testing::WithParamInterface<JsonFormatInput>
{
};

TEST_P(JsonFormatting, validateFormat)
{
    const JsonFormatInput input = GetParam();
    EXPECT_STREQ(
        formatJsonString(input.unformattedJson.toUtf8()).data(),
        input.formattedJson.toStdString().c_str());
}

static std::vector<JsonFormatInput> kJsonFormatInput {
    {
        "{}",
        "{}",
    },
    {
        "[]",
        "[]",
    },
    {
        "{",
        "{",
    },
    {
        "[",
        "[",
    },
    {
        "}",
        "\n}"
    },
    {
        "]",
        "\n]"
    },
    {
        "{{",
/*suppress newline*/ 1 + (const char*) R"json(
{
    {)json"
    },
    {
R"json({"someKey":0})json",
/*suppress newline*/ 1 + (const char*) R"json(
{
    "someKey": 0
})json"
    },
    {
R"json({"someKey\"":0})json",
/*suppress newline*/ 1 + (const char*) R"json(
{
    "someKey\"": 0
})json"
    },
    {
R"json({"someKey\\\"":0})json",
/*suppress newline*/ 1 + (const char*) R"json(
{
    "someKey\\\"": 0
})json"
    },
    {
R"json("someKey":{"foo":"bar"})json",
/*suppress newline*/ 1 + (const char*) R"json(
"someKey": {
    "foo": "bar"
})json"
    },
    {
R"json({"someKey: [ 3, 4, 5]":0})json",
/*suppress newline*/ 1 + (const char*) R"json(
{
    "someKey: [ 3, 4, 5]": 0
})json"
    },
    {
R"json({"someKey\"{":0})json",
/*suppress newline*/ 1 + (const char*) R"json(
{
    "someKey\"{": 0
})json"
    },
    {
R"json({"random":"10","random float":"37.234","bool":"true","date":"1991-06-12","regEx":"ooooooooooooooooooooooooooooooooooo what","enum":"generator","firstname":"Priscilla","lastname":"Wu","city":"Forked River","country":"Lebanon","countryCode":"GQ","email uses current data":"Priscilla.Wu@gmail.com","email from expression":"Priscilla.Wu@yopmail.com","array":["Adore","Orsola","Elka","Edyth","Bobbi"],"array of objects":[{"index":"0","index start at 5":"5"},{"index":"1","index start at 5":"6"},{"index":"2","index start at 5":"7"}],"Ardeen":{"age":"83"}})json",
/*suppress newline*/ 1 + (const char*) R"json(
{
    "random": "10",
    "random float": "37.234",
    "bool": "true",
    "date": "1991-06-12",
    "regEx": "ooooooooooooooooooooooooooooooooooo what",
    "enum": "generator",
    "firstname": "Priscilla",
    "lastname": "Wu",
    "city": "Forked River",
    "country": "Lebanon",
    "countryCode": "GQ",
    "email uses current data": "Priscilla.Wu@gmail.com",
    "email from expression": "Priscilla.Wu@yopmail.com",
    "array": [
        "Adore",
        "Orsola",
        "Elka",
        "Edyth",
        "Bobbi"
    ],
    "array of objects": [
        {
            "index": "0",
            "index start at 5": "5"
        },
        {
            "index": "1",
            "index start at 5": "6"
        },
        {
            "index": "2",
            "index start at 5": "7"
        }
    ],
    "Ardeen": {
        "age": "83"
    }
})json"
    },
    {
/*suppress newline*/ 1 + (const char*) R"json(
{
    "already":     "slightly",
    "formatted": [
      "input"   ,
      "with",
      "bad whitespace"
    ]
})json",
/*suppress newline*/ 1 + (const char*) R"json(
{
    "already": "slightly",
    "formatted": [
        "input",
        "with",
        "bad whitespace"
    ]
})json",
    },
    {
/*suppress newline*/ 1 + (const char*) R"json(
{
    "emptyContainerButWhiteSpace": [    ],
    "anotherEmpty": {

    }
})json",
/*suppress newline*/ 1 + (const char*) R"json(
{
    "emptyContainerButWhiteSpace": [],
    "anotherEmpty": {}
})json",
    },
};

INSTANTIATE_TEST_SUITE_P(String, JsonFormatting, ::testing::ValuesIn(kJsonFormatInput));

} // namespace nx::utils::test
