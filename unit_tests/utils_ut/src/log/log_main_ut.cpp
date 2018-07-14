#include <gtest/gtest.h>

#include <nx/utils/log/log_initializer.h>
#include <nx/utils/log/log_main.h>
#include <nx/utils/log/log_settings.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/test_support/test_options.h>

#include <QtCore/QSize>

namespace nx {
namespace utils {
namespace log {
namespace test {

static const Tag kTestTag(lit("TestTag"));
static const Tag kNamespaceTag(lit("nx::utils::log::test"));

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
        settings.loggers.front().level.filters = LevelFilters{{kNamespaceTag, Level::verbose}};

        if (!logWriter)
            logWriter = std::unique_ptr<AbstractWriter>(buffer = new Buffer);

        log::addLogger(buildLogger(
            settings,
            QString("log_ut"),
            QString(),
            {kTestTag, kNamespaceTag},
            std::move(logWriter)));

        // Ignoring initialization messages.
        if (buffer)
            buffer->clear();

        EXPECT_EQ(Level::verbose, maxLevel());
    }

    ~LogMainTest()
    {
        removeLoggers({kTestTag, kNamespaceTag});
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

    Buffer* buffer = nullptr;
    const Level initialLevel;
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
}

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
