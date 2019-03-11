#include <gtest/gtest.h>

#include <thread>

#define NX_DEBUG_ENABLE_OUTPUT false //< Set to true to enable verbose output of this test.
#include <nx/kit/debug.h>

#include <nx/utils/log/log_initializer.h>
#include <nx/utils/log/log_main.h>
#include <nx/utils/log/log_settings.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/test_support/test_options.h>
#include <nx/utils/unused.h>

#include <QtCore/QSize>

namespace nx {
namespace utils {
namespace log {
namespace test {

static const Tag kTestTag(QLatin1String("TestTag"));
static const Filter kTestFilter(kTestTag);
static const Filter kNamespaceFilter(QLatin1String("nx"));

class LogMainTest: public ::testing::Test
{
public:
    LogMainTest(std::unique_ptr<AbstractWriter> logWriter = nullptr):
        initialLevel(mainLogger()->defaultLevel())
    {
        log::unlockConfiguration();

        mainLogger()->setDefaultLevel(Level::none);

        log::Settings settings;
        settings.loggers.resize(1);
        settings.loggers.front().level.primary = levelFromString("INFO");
        settings.loggers.front().level.filters = LevelFilters{{kNamespaceFilter, Level::verbose}};

        if (!logWriter)
            logWriter = std::unique_ptr<AbstractWriter>(buffer = new Buffer);

        log::addLogger(buildLogger(
            settings,
            QString("log_ut"),
            QString(),
            {kTestFilter, kNamespaceFilter},
            std::move(logWriter)));

        // Ignoring initialization messages.
        if (buffer)
            buffer->clear();

        EXPECT_EQ(Level::verbose, maxLevel());
    }

    ~LogMainTest()
    {
        removeLoggers({kTestFilter, kNamespaceFilter});
        mainLogger()->setDefaultLevel(initialLevel);

        log::lockConfiguration();
    }

    void expectMessages(const std::vector<const char*>& patterns)
    {
        const auto messages = buffer->takeMessages();
        ASSERT_EQ(patterns.size(), messages.size());
        for (size_t i = 0; i < patterns.size(); ++i)
        {
             const QRegExp regExp(QString::fromUtf8(patterns[i]),
                 Qt::CaseSensitive, QRegExp::Wildcard);

             EXPECT_TRUE(regExp.exactMatch(messages[i]))
                 << "Line [" << messages[i].toStdString() << "]" << std::endl
                 << "    does not match pattern [" << patterns[i] << "]";
        }
    }

    void setIniValue(const int* iniField, int newValue)
    {
        const auto oldValue = *iniField;
        int* mutableField = const_cast<int*>(iniField);
        iniGuards.push_back(makeSharedGuard([=](){ *mutableField = oldValue; }));
        *mutableField = newValue;
    }

    Buffer* buffer = nullptr;
    const Level initialLevel;
    std::vector<SharedGuardPtr> iniGuards;
};

TEST_F(LogMainTest, ExplicitTag)
{
    NX_ALWAYS(kTestTag, "Always");
    NX_ERROR(kTestTag, "Error");
    NX_WARNING(kTestTag, "Warning");
    NX_INFO(kTestTag, "Info");
    NX_DEBUG(kTestTag, "Debug");
    NX_VERBOSE(kTestTag, "Verbose");
    expectMessages({
        "* ALWAYS TestTag: Always",
        "* ERROR TestTag: Error",
        "* WARNING TestTag: Warning",
        "* INFO TestTag: Info"});

    const int kSeven = 7;
    NX_ALWAYS(kTestTag) << "Always" << kSeven;
    NX_INFO(kTestTag) << "Info" << kSeven;
    NX_VERBOSE(kTestTag) << "Verbose" << kSeven;
    expectMessages({
        "* ALWAYS TestTag: Always 7",
        "* INFO TestTag: Info 7"});

    NX_ERROR(kTestTag, "Value %1 = %2", "error_count", 1);
    NX_WARNING(kTestTag, "Value %1 = %2", "error_count", 2);
    NX_DEBUG(kTestTag, "Value %1 = %2", "error_count", 3);
    expectMessages({
        "* ERROR TestTag: Value error_count = 1",
        "* WARNING TestTag: Value error_count = 2"});
}

TEST_F(LogMainTest, This)
{
    NX_ALWAYS(this, "Always");
    NX_ERROR(this, "Error");
    NX_WARNING(this, "Warning");
    NX_INFO(this, "Info");
    NX_DEBUG(this, "Debug");
    NX_VERBOSE(this, "Verbose");
    expectMessages({
        "* ALWAYS nx::utils::log::test::*LogMain*(0x*): Always",
        "* ERROR nx::utils::log::test::*LogMain*(0x*): Error",
        "* WARNING nx::utils::log::test::*LogMain*(0x*): Warning",
        "* INFO nx::utils::log::test::*LogMain*(0x*): Info",
        "* DEBUG nx::utils::log::test::*LogMain*(0x*): Debug",
        "* VERBOSE nx::utils::log::test::*LogMain*(0x*): Verbose"});

    const QSize kSize(2, 3);
    NX_ALWAYS(this) << "Always" << kSize;
    NX_INFO(this) << "Info" << kSize;
    NX_VERBOSE(this) << "Verbose" << kSize;
    expectMessages({
        "* ALWAYS nx::utils::log::test::*LogMain*(0x*): Always QSize(2, 3)",
        "* INFO nx::utils::log::test::*LogMain*(0x*): Info QSize(2, 3)",
        "* VERBOSE nx::utils::log::test::*LogMain*(0x*): Verbose QSize(2, 3)"});

    NX_ERROR(this, "Value %1 = %2", "error_count", 1);
    NX_WARNING(this, "Value %1 = %2", "error_count", 2);
    NX_DEBUG(this, "Value %1 = %2", "error_count", 3);
    expectMessages({
        "* ERROR nx::utils::log::test::*LogMain*(0x*): Value error_count = 1",
        "* WARNING nx::utils::log::test::*LogMain*(0x*): Value error_count = 2",
        "* DEBUG nx::utils::log::test::*LogMain*(0x*): Value error_count = 3"});
}

TEST_F(LogMainTest, LevelReducer)
{
    nx::utils::test::ScopedTimeShift timeShift(nx::utils::test::ClockType::steady);

    const auto logError = [this]() { NX_ERROR(this, "Error"); };
    const auto logWarning = [this]() { NX_WARNING(this, "Warning"); };
    const auto logInfo = [this]() { NX_INFO(this, "Info"); };

    setIniValue(&ini().logLevelReducerPassLimit, 2);
    setIniValue(&ini().logLevelReducerWindowSizeS, 60);

    for (int i = 0; i != 4; ++i)
        logWarning();

    expectMessages({
        "* WARNING *: Warning",
        "* WARNING *: TOO MANY MESSAGES: Warning",
        "* DEBUG *: Warning",
        "* DEBUG *: Warning"
    });

    logWarning();
    timeShift.applyRelativeShift(std::chrono::minutes(1));
    logWarning();

    expectMessages({
        "* DEBUG *: Warning",
        "* WARNING *: Warning",
    });

    for (int i = 0; i != 3; ++i)
    {
        logError();
        logInfo();
    }

    expectMessages({
        "* ERROR *: Error",
        "* INFO *: Info",
        "* ERROR *: TOO MANY MESSAGES: Error",
        "* INFO *: Info",
        "* DEBUG *: Error",
        "* INFO *: Info",
    });
}

TEST_F(LogMainTest, LevelReducerWithStream)
{
    nx::utils::test::ScopedTimeShift timeShift(nx::utils::test::ClockType::steady);
    const auto logWarning = [this]() { NX_WARNING(this) << "Warn"; };

    setIniValue(&ini().logLevelReducerPassLimit, 1);
    setIniValue(&ini().logLevelReducerWindowSizeS, 60);

    logWarning();
    logWarning();

    expectMessages({
        "* WARNING *: TOO MANY MESSAGES: Warn",
        "* DEBUG *: Warn",
    });

    logWarning();
    timeShift.applyRelativeShift(std::chrono::minutes(1));
    logWarning();

    expectMessages({
        "* DEBUG *: Warn",
        "* WARNING *: TOO MANY MESSAGES: Warn",
    });
}

//-------------------------------------------------------------------------------------------------
// NX_SCOPE_TAG tests.

#define TEST_NX_SCOPE_TAG(NAME) do \
{ \
    { \
        struct ScopeTag{}; /*< Needed for NX_SCOPE_TAG macro used in NX_OUTPUT. */ \
        NX_OUTPUT << "NX_SCOPE_TAG " << NAME << ": " \
            << nx::kit::utils::toString(NX_SCOPE_TAG.toString()); \
    } \
    NX_VERBOSE(NX_SCOPE_TAG, NAME); /*< Perform the tested action. */ \
} while (0)

/** Used to test logging from a global function. */
static /*dummyValue*/ std::string* globalFunction(int dummyInt, char dummyChar)
{
    nx::utils::unused(dummyInt, dummyChar);
    TEST_NX_SCOPE_TAG("globalFunction");
    return nullptr;
}

/** Used to test logging from a static function in a class template. */
template<typename T, char c>
struct StructTemplate
{
    static T staticFunction(T dummyT, char dummyChar = c)
    {
        nx::utils::unused(dummyT, dummyChar);
        TEST_NX_SCOPE_TAG("StructTemplate::staticFunction");
        return []()
        {
            TEST_NX_SCOPE_TAG("lambda in StructTemplate::staticFunction");
            return []()
            {
                TEST_NX_SCOPE_TAG("inner lambda in StructTemplate::staticFunction");
                return nullptr;
            }();
        }();
    }
};

/** Used for a pointer-to-member as a template non-type argument for functionTemplate<>(). */
struct DummyStruct
{
    short memberFunction(char c) { return /*dummy*/ -c; }
};

/** Used to test logging from a function template, and from a lambda. */
template<typename T, short (DummyStruct::*p)(char)>
T functionTemplate(T dummyT, char dummyChar = 'c')
{
    nx::utils::unused(dummyT, dummyChar);
    TEST_NX_SCOPE_TAG("functionTemplate");
    return []()
    {
        TEST_NX_SCOPE_TAG("lambda in functionTemplate");
        return []()
        {
            TEST_NX_SCOPE_TAG("inner lambda in functionTemplate");
            return nullptr;
        }();
    }();
}

TEST_F(LogMainTest, Scope)
{
    TEST_NX_SCOPE_TAG("method");
    expectMessages({"* VERBOSE nx::utils::log::test::LogMainTest_Scope_Test: method"});

    globalFunction(/*dummyInt*/ 42, /*dummyChar*/ 'x');
    expectMessages({"* VERBOSE nx::utils::log::test: globalFunction"});

    StructTemplate</*T*/ std::string*, /*c*/ '@'>::staticFunction(
        /*dummyT*/ nullptr, /*dummyChar*/ 'X');
    expectMessages({
        "* VERBOSE nx::utils::log::test::StructTemplate<*>: StructTemplate::staticFunction",
        "* VERBOSE nx::utils::log::test::StructTemplate<*>: lambda in StructTemplate::staticFunction",
        "* VERBOSE nx::utils::log::test::StructTemplate<*>: inner lambda in StructTemplate::staticFunction",
    });

    functionTemplate</*T*/ std::string*, /*c*/ &DummyStruct::memberFunction>(
        /*dummyT*/ nullptr, /*dummyChar*/ 'X');
    expectMessages({
        "* VERBOSE nx::utils::log::test: functionTemplate",
        "* VERBOSE nx::utils::log::test: lambda in functionTemplate",
        "* VERBOSE nx::utils::log::test: inner lambda in functionTemplate",
    });
}

//-------------------------------------------------------------------------------------------------

class LogMainPerformanceTest: public LogMainTest
{
    using base_type = LogMainTest;

public:
    LogMainPerformanceTest():
        base_type(std::make_unique<NullDevice>())
    {
        // FIXME: #mshevchenko Should we use setLevelFilters() here instead?
        // logger->setExceptionFilters({lit("nx::utils::log::test::Enabled")});
    }

    template<typename Action>
    void measureTime(const Action& action)
    {
        const auto repeatCount = TestOptions::applyLoadMode(1000);
        const auto startTime = std::chrono::steady_clock::now();
        for (int i = 0; i < repeatCount; ++i)
            action();

        const auto duration = std::chrono::steady_clock::now() - startTime;
        std::cout << "Run time: " << duration.count() << " " <<
            ::toString(typeid(duration)).toStdString() << std::endl;
    }

    template<template<int> class Tag>
    void measureTimeForTags()
    {
        measureTime(
            []()
            {
                { static const Tag<0> t{0}; NX_DEBUG(&t, "Test message"); }
                { static const Tag<1> t{0}; NX_DEBUG(&t, "Test message"); }
                { static const Tag<2> t{0}; NX_DEBUG(&t, "Test message"); }
                { static const Tag<3> t{0}; NX_DEBUG(&t, "Test message"); }
                { static const Tag<4> t{0}; NX_DEBUG(&t, "Test message"); }
                { static const Tag<5> t{0}; NX_DEBUG(&t, "Test message"); }
                { static const Tag<6> t{0}; NX_DEBUG(&t, "Test message"); }
                { static const Tag<7> t{0}; NX_DEBUG(&t, "Test message"); }
                { static const Tag<8> t{0}; NX_DEBUG(&t, "Test message"); }
                { static const Tag<9> t{0}; NX_DEBUG(&t, "Test message"); }
            });
    }
};

template<int Id> struct EnabledTag { int i; };
template<int Id> struct DisabledTag { int i; };

TEST_F(LogMainPerformanceTest, EnabledTag) { measureTimeForTags<EnabledTag>(); }
TEST_F(LogMainPerformanceTest, DisabledTag) { measureTimeForTags<DisabledTag>(); }

} // namespace test
} // namespace log
} // namespace utils
} // namespace nx
