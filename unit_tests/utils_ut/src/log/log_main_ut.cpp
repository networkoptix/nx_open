#include <gtest/gtest.h>

#include <nx/utils/log/log_main.h>
#include <QtCore/QSize>

namespace nx {
namespace utils {
namespace log {
namespace test {

class LogMainTest: public ::testing::Test
{
public:
    LogMainTest():
        initialLevel(mainLogger()->defaultLevel())
    {
        mainLogger()->setDefaultLevel(Level::none);

        logger = addLogger({lit("SomeTag"), lit("nx::utils::log::test")});
        EXPECT_EQ(Level::none, maxLevel());
        logger->setDefaultLevel(levelFromString("INFO"));
        EXPECT_EQ(Level::info, maxLevel());
        logger->setWriter(std::unique_ptr<AbstractWriter>(buffer = new Buffer));
        logger->setExceptionFilters({lit("nx::utils::log::test")});
        EXPECT_EQ(Level::verbose, maxLevel());
    }

    ~LogMainTest()
    {
        removeLoggers({lit("SomeTag"), lit("nx::utils::log::test")});
        mainLogger()->setDefaultLevel(initialLevel);
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

    std::shared_ptr<Logger> logger;
    Buffer* buffer;
    const Level initialLevel;
};

TEST_F(LogMainTest, ExplicitTag)
{
    const auto testTag = lit("SomeTag");
    NX_ALWAYS(testTag, "Always");
    NX_ERROR(testTag, "Error");
    NX_WARNING(testTag, "Warning");
    NX_INFO(testTag, "Info");
    NX_DEBUG(testTag, "Debug");
    NX_VERBOSE(testTag, "Verbose");
    expectMessages({
        "* ALWAYS SomeTag: Always",
        "* ERROR SomeTag: Error",
        "* WARNING SomeTag: Warning",
        "* INFO SomeTag: Info"});

    const int kSeven = 7;
    NX_ALWAYS(testTag) "Always" << kSeven;
    NX_INFO(testTag) "Info" << kSeven;
    NX_VERBOSE(testTag) "Verbose" << kSeven;
    expectMessages({
        "* ALWAYS SomeTag: Always 7",
        "* INFO SomeTag: Info 7"});
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

#if 0
    const QSize kSize(2, 3);
    NX_ALWAYS() << "Always" << kSize;
    NX_INFO() << "Info" << kSize;
    NX_VERBOSE() << "Verbose" << kSize;
    expectMessages({
        "* ALWAYS nx::utils::log::test::*LogMain*(0x*): Always QSize(2, 3)",
        "* ERROR nx::utils::log::test::*LogMain*(0x*): Info QSize(2, 3)",
        "* VERBOSE nx::utils::log::test::*LogMain*(0x*): Verbose QSize(2, 3)"});
#endif
}

} // namespace test
} // namespace log
} // namespace utils
} // namespace nx
