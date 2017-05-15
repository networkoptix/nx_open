#include <gtest/gtest.h>
#include <nx/utils/log/log_message.h>

#include <nx/utils/std/cpp14.h>
#include <nx/utils/uuid.h>

template<typename ... Args>
void testArgQString(const char* format, const Args& ... args)
{
    const QString result = lm(format).arg(args ...);
    EXPECT_EQ(QString::fromUtf8(format).arg(args ...), result);
}

template<typename ... Args>
void testArgsQString(const char* format, const Args& ... args)
{
    const QString result = lm(format).args(args ...);
    EXPECT_EQ(QString::fromUtf8(format).arg(args ...), result);
}

TEST(QnLogMessage, argQString)
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
    const QString result = lm("%1").arg(value);
    EXPECT_EQ(expected, result);
}

TEST(QnLogMessage, argCustom)
{
    testCustom(QLatin1String("hello"), QByteArray("hello"));
    testCustom(QLatin1String("positive"), std::string("positive"));
    testCustom(QLatin1String("{fbc94ac6-c305-11e6-9583-4b520c3a522d}"),
        QnUuid("fbc94ac6-c305-11e6-9583-4b520c3a522d"));

    testCustom(QLatin1String("234s"), std::chrono::seconds(234));
    testCustom(QLatin1String("342332ms"), std::chrono::milliseconds(342332));
    testCustom(QLatin1String("25623usec"), std::chrono::microseconds(25623));

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
    EXPECT_EQ(lm(format).args(args ...), QString::fromUtf8(format).arg(args ...));
}

struct SomeLongStructNameForThisTest {};
class SomeLongClassNameForThisTest {};

TEST(QnLogMessage, str)
{
    testStrQString("%1", QLatin1String("hello"));
    testStrQString("%1 %2", 123);
    testStrQString("%1 %2 %3", 123.456);
    testStrQString("%1 %2", QLatin1String("hello"), QLatin1String("world"));

    EXPECT_EQ(QLatin1String("QString"), lm("%1").arg(QString::fromUtf8("QString")));
    EXPECT_EQ(QLatin1String("MethodTester"), lm("%1").arg(MethodTester()));
    EXPECT_EQ(QLatin1String("FunctionTester"), lm("%1").arg(FunctionTester()));

    EXPECT_EQ(QLatin1String("A B C D"), lm("%1 %2 %3").arg("A").arg("B C").arg("D"));
    EXPECT_EQ(QLatin1String("same same"), lm("%1 %2").arg("%2").arg("same"));
    ASSERT_EQ(std::string("hello world %4 yes"),
        lm("%1 %2 %3 %4").args("hello", "world", "%4", "yes").toStdString());

    const auto obj1 = std::make_unique<SomeLongStructNameForThisTest>();
    const auto name1 = QLatin1String("SomeLongStructNameForThisTest(0x");
    EXPECT_TRUE(QString(lm("%1").arg(obj1)).startsWith(name1));
    EXPECT_TRUE(QString(lm("%1").arg(obj1.get())).startsWith(name1));

    const auto obj2 = std::make_shared<SomeLongClassNameForThisTest>();
    const auto name2 = QLatin1String("SomeLongClassNameForThisTest(0x");
    EXPECT_TRUE(QString(lm("%1").arg(obj2)).startsWith(name2));
    EXPECT_TRUE(QString(lm("%1").arg(obj2.get())).startsWith(name2));
}

