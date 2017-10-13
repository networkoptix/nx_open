#include <gtest/gtest.h>

#include <nx/utils/log/log_logger.h>
#include <nx/utils/std/cpp14.h>

#include <QtCore/QDateTime>

namespace nx {
namespace utils {
namespace log {
namespace test {

TEST(LogLogger, Levels)
{
    const auto buffer = new Buffer();
    const Logger::OnLevelChanged onLevelChanged = nullptr;

    Logger logger(onLevelChanged, Level::info, std::unique_ptr<AbstractWriter>(buffer));
    ASSERT_EQ(Level::info, logger.defaultLevel());

    EXPECT_TRUE(logger.isToBeLogged(Level::always));
    EXPECT_TRUE(logger.isToBeLogged(Level::error));
    EXPECT_TRUE(logger.isToBeLogged(Level::warning));
    EXPECT_TRUE(logger.isToBeLogged(Level::info));
    EXPECT_FALSE(logger.isToBeLogged(Level::debug));
    EXPECT_FALSE(logger.isToBeLogged(Level::verbose));

    logger.log(Level::error, QString(), "aaa");
    logger.log(Level::warning, QString(), "bbb");
    logger.log(Level::info, QString(), "ccc");
    logger.log(Level::debug, QString(), "ddd");
    logger.log(Level::verbose, QString(), "eee");
    ASSERT_EQ((size_t) 3, buffer->takeMessages().size());

    logger.setDefaultLevel(Level::error);
    ASSERT_EQ(Level::error, logger.defaultLevel());

    EXPECT_TRUE(logger.isToBeLogged(Level::always));
    EXPECT_TRUE(logger.isToBeLogged(Level::error));
    EXPECT_FALSE(logger.isToBeLogged(Level::warning));
    EXPECT_FALSE(logger.isToBeLogged(Level::info));
    EXPECT_FALSE(logger.isToBeLogged(Level::debug));
    EXPECT_FALSE(logger.isToBeLogged(Level::verbose));

    logger.log(Level::error, QString(), "aaa");
    logger.log(Level::warning, QString(), "bbb");
    logger.log(Level::info, QString(), "ccc");
    logger.log(Level::debug, QString(), "ddd");
    logger.log(Level::verbose, QString(), "eee");
    ASSERT_EQ((size_t) 1, buffer->takeMessages().size());
}

TEST(LogLogger, Filters)
{
    const auto buffer = new Buffer();
    const Logger::OnLevelChanged onLevelChanged = nullptr;

    Logger logger(onLevelChanged, Level::info, std::unique_ptr<AbstractWriter>(buffer));
    ASSERT_EQ((size_t) 0, logger.exceptionFilters().size());

    EXPECT_TRUE(logger.isToBeLogged(Level::warning, QLatin1String("nx::first::className1")));
    EXPECT_TRUE(logger.isToBeLogged(Level::warning, QLatin1String("nx::second::className2")));
    EXPECT_FALSE(logger.isToBeLogged(Level::debug, QLatin1String("nx::first::className3")));
    EXPECT_FALSE(logger.isToBeLogged(Level::debug, QLatin1String("nx::second::className4")));

    logger.log(Level::warning, QLatin1String("nx::first::className1"), "aaa");
    logger.log(Level::warning, QLatin1String("nx::second::className2"), "bbb");
    logger.log(Level::debug, QLatin1String("nx::first::className3"), "ccc");
    logger.log(Level::debug, QLatin1String("nx::second::className4"), "ddd");
    ASSERT_EQ((size_t) 2, buffer->takeMessages().size());

    logger.setExceptionFilters({QLatin1String("nx::first")});
    ASSERT_EQ((size_t) 1, logger.exceptionFilters().size());

    EXPECT_TRUE(logger.isToBeLogged(Level::warning, QLatin1String("nx::first::className1")));
    EXPECT_TRUE(logger.isToBeLogged(Level::warning, QLatin1String("nx::second::className2")));
    EXPECT_TRUE(logger.isToBeLogged(Level::debug, QLatin1String("nx::first::className3")));
    EXPECT_FALSE(logger.isToBeLogged(Level::debug, QLatin1String("nx::second::className4")));

    logger.log(Level::warning, QLatin1String("nx::first::className1"), "aaa");
    logger.log(Level::warning, QLatin1String("nx::second::className2"), "bbb");
    logger.log(Level::debug, QLatin1String("nx::first::className3"), "ccc");
    logger.log(Level::debug, QLatin1String("nx::second::className4"), "ddd");
    ASSERT_EQ((size_t) 3, buffer->takeMessages().size());

    logger.setExceptionFilters({QLatin1String("nx::second"), QLatin1String("nx::third")});
    ASSERT_EQ((size_t) 2, logger.exceptionFilters().size());

    EXPECT_TRUE(logger.isToBeLogged(Level::warning, QLatin1String("nx::first::className1")));
    EXPECT_TRUE(logger.isToBeLogged(Level::warning, QLatin1String("nx::second::className2")));
    EXPECT_FALSE(logger.isToBeLogged(Level::debug, QLatin1String("nx::first::className3")));
    EXPECT_TRUE(logger.isToBeLogged(Level::debug, QLatin1String("nx::second::className4")));

    logger.log(Level::warning, QLatin1String("nx::first::className1"), "aaa");
    logger.log(Level::warning, QLatin1String("nx::second::className2"), "bbb");
    logger.log(Level::debug, QLatin1String("nx::first::className3"), "ccc");
    logger.log(Level::debug, QLatin1String("nx::second::className4"), "ddd");
    ASSERT_EQ((size_t) 3, buffer->takeMessages().size());
}

TEST(LogLogger, Format)
{
    const auto buffer = new Buffer();
    const Logger::OnLevelChanged onLevelChanged = nullptr;
    Logger logger(onLevelChanged, Level::verbose, std::unique_ptr<AbstractWriter>(buffer));

    logger.log(Level::always, QLatin1String("nx::aaa::Object(1)"), "First message");
    logger.log(Level::error, QLatin1String("nx::bbb::Object(2)"), "Second message");
    logger.log(Level::warning, QLatin1String("nx::ccc::Object(3)"), "Third message");
    logger.log(Level::info, QLatin1String("nx::ddd::Object(4)"), "Forth message");
    logger.log(Level::debug, QLatin1String("nx::eee::Object(5)"), "Fivth message");
    logger.log(Level::verbose, QLatin1String("nx::fff::Object(6)"), "Sixth message");
    logger.log(Level::verbose, QString(), "Message without tag");

    const auto messages = buffer->takeMessages();
    ASSERT_EQ(7U, messages.size());

    const auto today = QDateTime::currentDateTime().toString("yyyy-MM-dd");
    const auto verifyMessage =
        [&](size_t index, const QString& line)
        {
            const auto& message = messages[index];
            EXPECT_TRUE(message.startsWith(today)) << message.toStdString();
            EXPECT_TRUE(message.endsWith(line)) << message.toStdString();
        };

    verifyMessage(0, QLatin1String("ALWAYS nx::aaa::Object(1): First message"));
    verifyMessage(1, QLatin1String("ERROR nx::bbb::Object(2): Second message"));
    verifyMessage(2, QLatin1String("WARNING nx::ccc::Object(3): Third message"));
    verifyMessage(3, QLatin1String("INFO nx::ddd::Object(4): Forth message"));
    verifyMessage(4, QLatin1String("DEBUG nx::eee::Object(5): Fivth message"));
    verifyMessage(5, QLatin1String("VERBOSE nx::fff::Object(6): Sixth message"));
    verifyMessage(6, QLatin1String("VERBOSE : Message without tag"));
}

} // namespace test
} // namespace log
} // namespace utils
} // namespace nx
