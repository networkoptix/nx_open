#include <gtest/gtest.h>
#include <nx/utils/log/log_message.h>

template<typename ... Args>
void testArgQString(const char* format, const Args& ... args)
{
    const QString result = lm(format).arg(args ...);
    EXPECT_EQ(QString::fromUtf8(format).arg(args ...), result);
}

TEST(QnLogMessage, argQString)
{
    testArgQString("%1", QLatin1String("hello"));

    testArgQString("%1 %2", 123);
    testArgQString("%1 %2", 123, 16, 10, QLatin1Char('0'));

    testArgQString("%1 %2 %3", 123.456);
    testArgQString("%1 %2 %3", 123.456, 10, 'E', 1, QLatin1Char('#'));

    testArgQString("%1 %2", QLatin1String("hello"), QLatin1String("world"));
    testArgQString("%1 %2 %3", QLatin1String("hello"), QLatin1String("world"), QLatin1String("!"));
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
    EXPECT_EQ(lm(format).arg(args ...), QString::fromUtf8(format).arg(args ...));
}

TEST(QnLogMessage, str)
{
    testStrQString("%1", QLatin1String("hello"));
    testStrQString("%1 %2", 123);
    testStrQString("%1 %2 %3", 123.456);
    testStrQString("%1 %2", QLatin1String("hello"), QLatin1String("world"));

    EXPECT_EQ(QLatin1String("QString"), lm("%1").str(QString::fromUtf8("QString")));
    EXPECT_EQ(QLatin1String("MethodTester"), lm("%1").str(MethodTester()));
    EXPECT_EQ(QLatin1String("FunctionTester"), lm("%1").str(FunctionTester()));

    EXPECT_EQ(QLatin1String("A B C D"), lm("%1 %2 %3").str("A").str("B C").str("D"));
    EXPECT_EQ(QLatin1String("same same"), lm("%1 %2").str("%2").str("same"));
    ASSERT_EQ("hello world %4 yes",
        lm("%1 %2 %3 %4").strs("hello", "world", "%4", "yes"));
}

