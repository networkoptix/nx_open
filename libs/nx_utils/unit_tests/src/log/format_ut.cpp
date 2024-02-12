// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

// TODO: Move to nx/formatter_ut.h

#include <gtest/gtest.h>
#include <nx/utils/log/format.h>

#include <nx/utils/std/cpp14.h>
#include <nx/utils/uuid.h>

template<typename ... Args>
void testArgQString(const char* format, const Args& ... args)
{
    const QString result = nx::format(format).arg(args ...);
    EXPECT_EQ(QString::fromUtf8(format).arg(args ...), result);
}

template<typename ... Args>
void testArgsQString(const char* format, const Args& ... args)
{
    const QString result = nx::format(format).args(args ...);
    EXPECT_EQ(QString::fromUtf8(format).arg(args ...), result);
}

TEST(Formatter, argQString)
{
    testArgQString("%1", QLatin1String("hello"));

    testArgQString("%1 %2", 123);
    testArgQString("%1 %2", 123, 16, 10, QLatin1Char('0'));

    testArgQString("%1 %2 %3", 123.456);
    testArgQString("%1 %2 %3", 123.456, 10, 'E', 1, QLatin1Char('#'));

    testArgsQString("%1 %2", QLatin1String("hello"), QLatin1String("world"));
    testArgsQString("%1 %2 %3", QLatin1String("hello"), QLatin1String("world"), QLatin1String("!"));
}

template<typename Value>
void testCustom(QString expected, const Value& value)
{
    const QString result = nx::format("%1").arg(value);
    EXPECT_EQ(expected, result);
}

TEST(Formatter, argCustom)
{
    testCustom(QLatin1String("hello"), QByteArray("hello"));
    testCustom(QLatin1String("positive"), std::string("positive"));
    testCustom(QLatin1String("{fbc94ac6-c305-11e6-9583-4b520c3a522d}"),
        nx::Uuid("fbc94ac6-c305-11e6-9583-4b520c3a522d"));

    testCustom(QLatin1String("234s"), std::chrono::seconds(234));
    testCustom(QLatin1String("342332ms"), std::chrono::milliseconds(342332));
    testCustom(QLatin1String("25623us"), std::chrono::microseconds(25623));

    testCustom(QLatin1String("Hello, world"), "Hello, world");
    testCustom(QLatin1String("Hello, world"), L"Hello, world");
}

namespace {

struct MethodTester
{
    QString toString() const
    {
        return QLatin1String("MethodTester");
    }
};

struct FunctionTester
{
};

static QString toString(const FunctionTester&)
{
    return QLatin1String("FunctionTester");
};

} // namespace

template<typename ... Args>
void testStrQString(const char* format, const Args& ... args)
{
    EXPECT_EQ(nx::format(format).args(args ...), QString::fromUtf8(format).arg(args ...));
}

struct SomeLongStructNameForThisTest {};
class SomeLongClassNameForThisTest {};

TEST(Formatter, str)
{
    testStrQString("%1", QLatin1String("hello"));
    testStrQString("%1 %2", 123);
    testStrQString("%1 %2 %3", 123.456);
    testStrQString("%1 %2", QLatin1String("hello"), QLatin1String("world"));

    EXPECT_EQ(QLatin1String("QString"), nx::format("%1").arg(QString::fromUtf8("QString")).toQString());
    EXPECT_EQ(QLatin1String("MethodTester"), nx::format("%1").arg(MethodTester()).toQString());
    EXPECT_EQ(QLatin1String("FunctionTester"), nx::format("%1").arg(FunctionTester()).toQString());

    EXPECT_EQ(QLatin1String("A B C D"), nx::format("%1 %2 %3").arg("A").arg("B C").arg("D").toQString());
    EXPECT_EQ(QLatin1String("same same"), nx::format("%1 %2").arg("%2").arg("same").toQString());
    ASSERT_EQ(std::string("hello world %4 yes"),
        nx::format("%1 %2 %3 %4").args("hello", "world", "%4", "yes").toStdString());

    const auto obj1 = std::make_unique<SomeLongStructNameForThisTest>();
    const auto name1 = QLatin1String("SomeLongStructNameForThisTest(0x");
    EXPECT_TRUE(QString(nx::format("%1").arg(obj1)).startsWith(name1));
    EXPECT_TRUE(QString(nx::format("%1").arg(obj1.get())).startsWith(name1));

    const auto obj2 = std::make_shared<SomeLongClassNameForThisTest>();
    const auto name2 = QLatin1String("SomeLongClassNameForThisTest(0x");
    EXPECT_TRUE(QString(nx::format("%1").arg(obj2)).startsWith(name2));
    EXPECT_TRUE(QString(nx::format("%1").arg(obj2.get())).startsWith(name2));
}

TEST(Formatter, NX_FMT)
{
    const auto expectEq = [](const QString& f, const QString& s) { EXPECT_EQ(f, s); };

    expectEq(NX_FMT(), "");
    expectEq(NX_FMT("hello"), "hello");
    expectEq(NX_FMT("value is %1", 777), "value is 777");
    expectEq(NX_FMT("%1 = %2", "number", 777), "number = 777");
}
