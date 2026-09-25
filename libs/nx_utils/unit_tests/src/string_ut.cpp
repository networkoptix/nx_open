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
#include <nx/utils/test_support/utils.h>

// clang-format off

namespace nx::utils::test {

TEST(GTestNameString, normalizesNonAlphanumericCharacters)
{
    EXPECT_EQ("server_a_camera_1", normalizedGTestNameString("server-a camera.1"));
}

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

namespace {

static int sign(int value)
{
    return (value > 0) - (value < 0);
}

// Strings exercising all token kinds (numbers, ASCII letters, runs of other characters) and
// their combinations, including cases that used to break the ordering.
static const QStringList kNaturalCompareSamples = {
    "", " ", "  ", "-", "- ", "_", "~", ".", "!", "+",
    "0", "00", "01", "1", "2", "9", "10", "11", "100",
    "1a", "1b", "2a", "10a", "a1", "a2", "a10", "a01",
    "a", "A", "b", "B", "z", "Z", "ab", "aB", "Ab",
    "-1", "-2", "-x", "-X", "- 1", "- x", "-_", "-~", "-1a",
    "1.5", "1.25", "1.", ".5", "1.5a", "1..2",
    "a b", "a-b", "a_b", "a 1", "a-1", "a_1", "a1b", "a1-b",
    "test", "test1", "test2", "test10", "test_1", "test_a", "test+a", "test!1",
    "Camera 1", "Camera 2", "Camera 10", "Camera 1a", "Camera 1-a", "Camera-1", "Camera - 1",
    "12345678901234567890", "12345678901234567891",
    QString("9").repeated(400), QString("9").repeated(401),
    QString::fromUtf16(u"\u041a\u0430\u043c\u0435\u0440\u0430 1"), //< Cyrillic "Kamera 1".
    QString::fromUtf16(u"\u041a\u0430\u043c\u0435\u0440\u0430 10"),
    QString::fromUtf16(u"\u0430\u0431\u0432"),
    QString::fromUtf16(u"\u0410\u0411\u0412"),
    QString::fromUtf16(u"\u0662"), //< Arabic-Indic digit two.
    QString::fromUtf16(u"1\u0661"), //< ASCII one followed by Arabic-Indic digit one.
    QString::fromUtf16(u"a\u00e9"),
    QString::fromUtf16(u"a\u00c9"),
};

static void checkStrictWeakOrdering(Qt::CaseSensitivity caseSensitivity, bool enableFloat)
{
    const auto& samples = kNaturalCompareSamples;
    const int n = (int) samples.size();

    std::vector<std::vector<int>> cmp(n, std::vector<int>(n));
    for (int i = 0; i < n; ++i)
    {
        for (int j = 0; j < n; ++j)
        {
            cmp[i][j] = sign(
                naturalStringCompare(samples[i], samples[j], caseSensitivity, enableFloat));
        }
    }

    for (int i = 0; i < n; ++i)
    {
        ASSERT_EQ(0, cmp[i][i]) << "Irreflexivity: \"" << samples[i].toStdString() << "\"";

        for (int j = 0; j < n; ++j)
        {
            ASSERT_EQ(cmp[i][j], -cmp[j][i]) << "Antisymmetry: \""
                << samples[i].toStdString() << "\" vs \"" << samples[j].toStdString() << "\"";
        }
    }

    // Transitivity of both "less" and "equivalent": cmp(a, b) == cmp(b, c) implies
    // cmp(a, c) is the same.
    for (int i = 0; i < n; ++i)
    {
        for (int j = 0; j < n; ++j)
        {
            for (int k = 0; k < n; ++k)
            {
                if (cmp[i][j] != cmp[j][k])
                    continue;

                ASSERT_EQ(cmp[i][j], cmp[i][k]) << "Transitivity: \""
                    << samples[i].toStdString() << "\", \""
                    << samples[j].toStdString() << "\", \""
                    << samples[k].toStdString() << "\"";
            }
        }
    }

    // Incomparability of a and b, and b being less than c, implies a is less than c.
    for (int i = 0; i < n; ++i)
    {
        for (int j = 0; j < n; ++j)
        {
            if (cmp[i][j] != 0)
                continue;

            for (int k = 0; k < n; ++k)
            {
                ASSERT_EQ(cmp[j][k], cmp[i][k]) << "Equivalence consistency: \""
                    << samples[i].toStdString() << "\" ~ \""
                    << samples[j].toStdString() << "\" vs \""
                    << samples[k].toStdString() << "\"";
            }
        }
    }
}

static QString naturalSorted(QStringList list, Qt::CaseSensitivity caseSensitivity)
{
    return naturalStringSort(list, caseSensitivity).join(", ");
}

} // namespace

TEST(NaturalStringCompare, strictWeakOrderingCaseSensitive)
{
    checkStrictWeakOrdering(Qt::CaseSensitive, /*enableFloat*/ false);
}

TEST(NaturalStringCompare, strictWeakOrderingCaseInsensitive)
{
    checkStrictWeakOrdering(Qt::CaseInsensitive, /*enableFloat*/ false);
}

TEST(NaturalStringCompare, strictWeakOrderingCaseSensitiveFloat)
{
    checkStrictWeakOrdering(Qt::CaseSensitive, /*enableFloat*/ true);
}

TEST(NaturalStringCompare, strictWeakOrderingCaseInsensitiveFloat)
{
    checkStrictWeakOrdering(Qt::CaseInsensitive, /*enableFloat*/ true);
}

TEST(NaturalStringCompare, numbersAreComparedByValue)
{
    ASSERT_LT(naturalStringCompare(u"a2", u"a10"), 0);
    ASSERT_GT(naturalStringCompare(u"a10", u"a9"), 0);
    ASSERT_EQ(naturalStringCompare(u"a01", u"a1"), 0);
    ASSERT_LT(naturalStringCompare(u"a1b2", u"a1b10"), 0);
    ASSERT_EQ("Camera 1, Camera 2, Camera 10, Camera 100",
        naturalSorted({"Camera 100", "Camera 10", "Camera 2", "Camera 1"}, Qt::CaseSensitive));
}

TEST(NaturalStringCompare, numberFollowedByLetter)
{
    // Used to be a cycle: "1a" < "2" < "10" < "1a".
    ASSERT_LT(naturalStringCompare(u"1a", u"2"), 0);
    ASSERT_LT(naturalStringCompare(u"2", u"10"), 0);
    ASSERT_LT(naturalStringCompare(u"1a", u"10"), 0);
    ASSERT_EQ("Camera 1, Camera 1a, Camera 1b, Camera 2, Camera 10, Camera 10a",
        naturalSorted({"Camera 10a", "Camera 2", "Camera 1b", "Camera 10", "Camera 1a",
            "Camera 1"}, Qt::CaseSensitive));
}

TEST(NaturalStringCompare, separatorPrefix)
{
    // Used to be a cycle: "- 1" < "-1" < "-x" < "- 1".
    ASSERT_LT(naturalStringCompare(u"-1", u"-x"), 0);
    ASSERT_LT(naturalStringCompare(u"-x", u"- 1"), 0);
    ASSERT_LT(naturalStringCompare(u"-1", u"- 1"), 0);
}

TEST(NaturalStringCompare, numbersGoBeforeOtherCharacters)
{
    ASSERT_EQ("test, test1, test2, test10, test!1, test+a, test_1, test_a, test_b",
        naturalSorted({"test", "test2", "test1", "test10", "test_1", "test_a", "test_b", "test+a",
            "test!1"}, Qt::CaseInsensitive));
}

TEST(NaturalStringCompare, caseSensitivity)
{
    ASSERT_EQ(naturalStringCompare(u"abc", u"ABC", Qt::CaseInsensitive), 0);
    ASSERT_NE(naturalStringCompare(u"abc", u"ABC", Qt::CaseSensitive), 0);
    ASSERT_LT(naturalStringCompare(u"a2", u"A10", Qt::CaseInsensitive), 0);
}

TEST(NaturalStringCompare, floatNumbers)
{
    ASSERT_GT(
        naturalStringCompare(u"a1.5", u"a1.25", Qt::CaseSensitive, /*enableFloat*/ true), 0);
    ASSERT_LT(
        naturalStringCompare(u"a1.5", u"a1.25", Qt::CaseSensitive, /*enableFloat*/ false), 0);
}

TEST(NaturalStringCompare, hugeNumbers)
{
    const QString huge = QString("9").repeated(400);
    ASSERT_GT(naturalStringCompare(huge, u"10"), 0);
    ASSERT_LT(naturalStringCompare(u"10", huge), 0);
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

//-------------------------------------------------------------------------------------------------

struct QuoteDelimitedTokensInput
{
    QString input;
    QStringList delimiters;
    QString expected;
};

class QuoteDelimitedTokens:
    public ::testing::Test,
    public ::testing::WithParamInterface<QuoteDelimitedTokensInput>
{
};

TEST_P(QuoteDelimitedTokens, validateFormat)
{
    const QuoteDelimitedTokensInput input = GetParam();
    EXPECT_EQ(
        quoteDelimitedTokens(input.input, input.delimiters),
        input.expected);
}

static std::vector<QuoteDelimitedTokensInput> kQuoteDelimitedTokensInput{
    // Basic cases
    {
        "", // empty input
        {"OR", "AND"},
        ""
    },
    {
        "simple", // single word
        {"OR", "AND"},
        "simple"
    },
    {
        "hello world", // two words
        {"OR", "AND"},
        "\"hello world\""
    },

    // Cases with delimiters
    {
        "hello world OR test case", // basic OR case
        {"OR", "AND"},
        "\"hello world\" OR \"test case\""
    },
    {
        "a b AND c d OR e f", // multiple delimiters
        {"OR", "AND"},
        "\"a b\" AND \"c d\" OR \"e f\""
    },

    // Cases with quotes
    {
        "\"quoted text\" OR simple", // already quoted
        {"OR", "AND"},
        "\"quoted text\" OR simple"
    },
    {
        "\"hello world\" AND \"test case\"", // all quoted
        {"OR", "AND"},
        "\"hello world\" AND \"test case\""
    },

    // Custom delimiters
    {
        "a b XOR c d", // custom delimiter
        {"XOR"},
        "\"a b\" XOR \"c d\""
    },
    {
        "word1 PLUS word2 MINUS word3", // multiple custom delimiters
        {"PLUS", "MINUS"},
        "word1 PLUS word2 MINUS word3"
    },

    // Edge cases
    {
        "OR AND", // only delimiters
        {"OR", "AND"},
        "OR AND"
    },
    {
        "\"complex quoted\" OR \"text with AND\" AND simple", // nested delimiters in quotes
        {"OR", "AND"},
        "\"complex quoted\" OR \"text with AND\" AND simple"
    },
    {
        "hello   world    OR    test", // multiple spaces
        {"OR"},
        "\"hello world\" OR test"
    },

    // Mixed quoted and unquoted cases
    {
        "\"first part\" second part OR test", // mixed quotes before delimiter
        {"OR"},
        "\"\"first part\" second part\" OR test"
    },
    {
        "test OR \"quoted part\" unquoted part", // mixed quotes after delimiter
        {"OR"},
        "test OR \"\"quoted part\" unquoted part\""
    },
    {
        "a \"b c\" d AND \"e f\" g h OR i", // mixed quotes with multiple delimiters
        {"AND", "OR"},
        "\"a \"b c\" d\" AND \"\"e f\" g h\" OR i"
    },
    {
        "\"a b\" c d XOR e \"f g\" h", // mixed quotes on both sides
        {"XOR"},
        "\"\"a b\" c d\" XOR \"e \"f g\" h\""
    },

    // Multiple spaces cases
    {
        "\"quoted   text     here\" OR test", // multiple spaces in quotes
        {"OR"},
        "\"quoted   text     here\" OR test"  // preserve multiple spaces in quotes
    },
    {
        "hello   world     test OR simple", // multiple spaces without quotes
        {"OR"},
        "\"hello world test\" OR simple"   // normalize multiple spaces
    },
    {
        "test OR \"quoted    part\"   \"second     part\"", // varying spaces
        {"OR"},
        "test OR \"\"quoted    part\" \"second     part\"\"" // preserve in quotes, normalize between
    },
    {
        "test OR \"quoted    part\"\"second     part\"", // without space
        {"OR"},
        "test OR \"\"quoted    part\"\"second     part\"\"" // preserve in quotes
    },
    {
        "\"text   with    spaces\" AND normal   text    here", // mixed multiple spaces
        {"AND"},
        "\"text   with    spaces\" AND \"normal text here\"" // preserve in quotes, normalize outside
    }
};

INSTANTIATE_TEST_SUITE_P(String, QuoteDelimitedTokens,
    ::testing::ValuesIn(kQuoteDelimitedTokensInput));

} // namespace nx::utils::test
