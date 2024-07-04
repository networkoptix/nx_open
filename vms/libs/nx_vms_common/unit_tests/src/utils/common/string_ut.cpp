// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <QtCore/QDateTime>
#include <QtCore/QString>

#include <nx/fusion/model_functions.h>
#include <nx/string.h>
#include <nx/utils/datetime.h>
#include <nx/utils/string.h>
#include <nx/vms/event/helpers.h>

TEST( parseDateTime, general )
{
    const QString testDateStr( "2015-01-01T12:00:01" );
    static const qint64 USEC_PER_MS = 1000;
    static const qint64 MSEC_PER_SEC = 1000;

    const QDateTime testDate = QDateTime::fromString( testDateStr, Qt::ISODate );
    const qint64 testTimestamp = testDate.toMSecsSinceEpoch();
    const qint64 testTimestampUSec = testTimestamp * USEC_PER_MS;

    ASSERT_EQ( nx::utils::parseDateTimeUsec( testDateStr ), testTimestampUSec );
    ASSERT_EQ( nx::utils::parseDateTimeUsec( QString::number(testTimestamp) ), testTimestampUSec );
}

TEST(removeMnemonics, general)
{
    static const auto source = lit("& && &a &&a &&& b&b &a a& a&");
    static const auto changed = nx::utils::removeMnemonics(source);
    static const auto target = lit("& && a &&a &&& bb a a& a&");
    ASSERT_EQ(changed, target);
}

TEST(splitOnPureKeywords, fixture)
{
    struct Test
    {
        QString value;
        QStringList withEmptyParts;
        QStringList pure;
    };
    std::vector<Test> tests = {
        {"", {}, {}},
        {" ", {"", ""}, {}},
        {" a", {"", "a"}, {"a"}},

        {"\"\"", {"\"\""}, {}},
        {" \"\" ", {"", "\"\"", ""}, {}},
        {"\" \"", {"\" \""}, {" "}},
        {" \" \" ", {"", "\" \"", ""}, {" "}},
        {"\"\"a", {"\"\"a"}, {"\"\"a"}},
        {"\"a\"", {"\"a\""}, {"a"}},
        {"\"a\" ", {"\"a\"", ""}, {"a"}},

        {"\"", {"\""}, {"\""}},
        {" \" ", {"", "\" "}, {"\""}},
        {" \" a", {"", "\" a"}, {"\" a"}},

        {"a b c", {"a", "b", "c"}, {"a", "b", "c"}},
        {" a  b  c ", {"", "a", "", "b", "", "c", ""}, {"a", "b", "c"}},
        {"a \"b c\"", {"a", "\"b c\""}, {"a", "b c"}},
        {"a  \" b  c \"", {"a", "", "\" b  c \""}, {"a", " b  c "}},

        {"\"\" a b c", {"\"\"", "a", "b", "c"}, {"a", "b", "c"}},
    };
    for (const auto& t : tests)
    {
        ASSERT_EQ(nx::utils::smartSplit(t.value, ' '), t.withEmptyParts) << nx::String(t.value);
        ASSERT_EQ(nx::vms::event::splitOnPureKeywords(t.value), t.pure) << nx::String(t.value);
    }
}

TEST(checkForKeywords, fixture)
{
    using namespace nx::vms::event;

    ASSERT_TRUE(checkForKeywords("", splitOnPureKeywords("")));
    ASSERT_TRUE(checkForKeywords("a", splitOnPureKeywords("")));

    ASSERT_TRUE(checkForKeywords("a", splitOnPureKeywords("a b")));
    ASSERT_TRUE(checkForKeywords("b", splitOnPureKeywords("a b")));

    ASSERT_FALSE(checkForKeywords("", splitOnPureKeywords("a b")));
    ASSERT_FALSE(checkForKeywords("c", splitOnPureKeywords("a b")));
}

namespace nx::test {

struct Foo
{
    nx::String a;

    bool operator==(const Foo& right) const
    {
        return a == right.a;
    }
};

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(Foo, (json), (a))

TEST(String, serialized_as_a_string_not_buffer)
{
    Foo foo{"Hello, world"};

    const auto serialized = QJson::serialized(foo);
    ASSERT_EQ("{\"a\":\"Hello, world\"}", serialized);

    auto deserialized = QJson::deserialized<Foo>(serialized);
    ASSERT_EQ(foo, deserialized);
}

} // namespace nx::test
