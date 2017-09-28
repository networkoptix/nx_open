#include <gtest/gtest.h>

#include <nx/utils/log/log_main.h>
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
    LogMainTest()
    {
        logger = addLogger({kTestTag, kNamespaceTag});
        logger->setDefaultLevel(levelFromString("INFO"));
        logger->setWriter(std::unique_ptr<AbstractWriter>(buffer = new Buffer));
        logger->setLevelFilters(LevelFilters{{kNamespaceTag, Level::verbose}});
    }

    ~LogMainTest()
    {
        removeLoggers({kTestTag, kNamespaceTag});
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
                 << "Line: " << messages[i].toStdString() << std::endl
                 << "Does not match pattern: " << patterns[i];
        }
    }

    std::shared_ptr<Logger> logger;
    Buffer* buffer;
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
        "* INFO TestTag: Info"});const int kSeven = 7;

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

} // namespace test
} // namespace log
} // namespace utils
} // namespace nx
