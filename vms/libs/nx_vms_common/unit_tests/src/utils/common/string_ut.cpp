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

    const QDateTime testDate = QDateTime::fromString( testDateStr, Qt::ISODate );
    const auto testTimestamp = std::chrono::milliseconds{testDate.toMSecsSinceEpoch()};
    const auto testTimestampUSec = std::chrono::microseconds{testTimestamp};

    ASSERT_EQ( nx::utils::parseDateTimeUsec( testDateStr ), testTimestampUSec );
    ASSERT_EQ( nx::utils::parseDateTimeUsec( QString::number(testTimestamp.count()) ), testTimestampUSec );
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

TEST(String, createCollatorCorrectOrder)
{
    const auto collator = utils::createCollator(
        Qt::CaseInsensitive, /*numericMode*/ true, QLocale(QLocale::English));
    const auto sorted = [&collator](QStringList list)
    {
        std::ranges::sort(list,
            [&collator](const QString& l, const QString& r)
            { return collator.compare(l, r) < 0; });
        return list.join(", ");
    };

    ASSERT_EQ("test, test1", sorted({"test1", "test"}));
    ASSERT_EQ("test, test_1", sorted({"test_1", "test"}));
    ASSERT_EQ("test_1, test_a", sorted({"test_a", "test_1"}));
    ASSERT_EQ("test!, test!2", sorted({"test!2", "test!"}));
    ASSERT_EQ("test-1, test+1", sorted({"test+1", "test-1"}));
    ASSERT_EQ("CAM1, cam2, Cam10", sorted({"Cam10", "cam2", "CAM1"}));
    ASSERT_EQ("10.0.0.1, 192.168.0.9, 192.168.0.10",
        sorted({"192.168.0.10", "192.168.0.9", "10.0.0.1"}));
    ASSERT_EQ("test, test_1, test_a, test_b, test!1, test+a, test1, test2, test10",
        sorted({"test",
            "test2",
            "test1",
            "test10",
            "test_1",
            "test_a",
            "test_b",
            "test+a",
            "test!1"}));
}

} // namespace nx::test
