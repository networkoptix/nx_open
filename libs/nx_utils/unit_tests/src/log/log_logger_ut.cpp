// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/utils/log/log_logger.h>
#include <nx/utils/std/cpp14.h>

#include <QtCore/QDateTime>

namespace nx::log::test {

TEST(LogLogger, Levels)
{
    const auto buffer = new Buffer();
    int levelChangedCount = 0;
    const Logger::OnLevelChanged onLevelChanged = [&levelChangedCount](){ ++levelChangedCount; };

    Logger logger(
        std::set<Filter>(),
        Level::info,
        std::unique_ptr<AbstractWriter>(buffer));
    logger.setOnLevelChanged(onLevelChanged);
    ASSERT_EQ(Level::info, logger.defaultLevel());

    EXPECT_TRUE(logger.isToBeLogged(Level::error));
    EXPECT_TRUE(logger.isToBeLogged(Level::warning));
    EXPECT_TRUE(logger.isToBeLogged(Level::info));
    EXPECT_FALSE(logger.isToBeLogged(Level::debug));
    EXPECT_FALSE(logger.isToBeLogged(Level::verbose));
    EXPECT_FALSE(logger.isToBeLogged(Level::trace));

    logger.log(Level::error, Tag(), "aaa");
    logger.log(Level::warning, Tag(), "bbb");
    logger.log(Level::info, Tag(), "ccc");
    logger.log(Level::debug, Tag(), "ddd");
    logger.log(Level::verbose, Tag(), "eee");
    logger.log(Level::trace, Tag(), "fff");
    ASSERT_EQ((size_t) 3, buffer->takeMessages().size());

    EXPECT_EQ(0, levelChangedCount);
    logger.setDefaultLevel(Level::error);
    EXPECT_EQ(1, levelChangedCount);
    ASSERT_EQ(Level::error, logger.defaultLevel());

    EXPECT_TRUE(logger.isToBeLogged(Level::error));
    EXPECT_FALSE(logger.isToBeLogged(Level::warning));
    EXPECT_FALSE(logger.isToBeLogged(Level::info));
    EXPECT_FALSE(logger.isToBeLogged(Level::debug));
    EXPECT_FALSE(logger.isToBeLogged(Level::verbose));
    EXPECT_FALSE(logger.isToBeLogged(Level::trace));

    logger.log(Level::error, Tag(), "aaa");
    logger.log(Level::warning, Tag(), "bbb");
    logger.log(Level::info, Tag(), "ccc");
    logger.log(Level::debug, Tag(), "ddd");
    logger.log(Level::verbose, Tag(), "eee");
    logger.log(Level::trace, Tag(), "fff");
    ASSERT_EQ((size_t) 1, buffer->takeMessages().size());

    EXPECT_EQ(1, levelChangedCount);
}

static Tag makeTag(const char* tag) { return Tag(QString::fromUtf8(tag)); }
static Filter makeFilter(const char* filter) { return Filter(QString::fromUtf8(filter)); }

TEST(LogLogger, Filters)
{
    const auto buffer = new Buffer();
    int levelChangedCount = 0;
    const Logger::OnLevelChanged onLevelChanged = [&levelChangedCount](){ ++levelChangedCount; };

    Logger logger(
        std::set<Filter>(),
        Level::info,
        std::unique_ptr<AbstractWriter>(buffer));
    logger.setOnLevelChanged(onLevelChanged);
    EXPECT_EQ(0, levelChangedCount);
    ASSERT_EQ((size_t) 0, logger.levelFilters().size());
    EXPECT_EQ(logger.maxLevel(), Level::info);

    EXPECT_TRUE(logger.isToBeLogged(Level::warning, makeTag("nx::first::className1")));
    EXPECT_TRUE(logger.isToBeLogged(Level::warning, makeTag("nx::second::className2")));
    EXPECT_FALSE(logger.isToBeLogged(Level::debug, makeTag("nx::first::className3")));
    EXPECT_FALSE(logger.isToBeLogged(Level::debug, makeTag("nx::second::className4")));

    logger.log(Level::warning, makeTag("nx::first::className1"), "aaa");
    logger.log(Level::warning, makeTag("nx::second::className2"), "bbb");
    logger.log(Level::debug, makeTag("nx::first::className3"), "ccc");
    logger.log(Level::debug, makeTag("nx::second::className4"), "ddd");
    ASSERT_EQ((size_t) 2, buffer->takeMessages().size());

    logger.setLevelFilters(LevelFilters{{makeFilter("nx::third"), Level::debug}});
    ASSERT_EQ((size_t) 1, logger.levelFilters().size());
    EXPECT_EQ(logger.maxLevel(), Level::debug);

    logger.setLevelFilters(LevelFilters{{makeFilter("nx::first"), Level::verbose}});
    ASSERT_EQ((size_t) 1, logger.levelFilters().size());
    EXPECT_EQ(logger.maxLevel(), Level::verbose);

    EXPECT_TRUE(logger.isToBeLogged(Level::warning, makeTag("nx::first::className1")));
    EXPECT_TRUE(logger.isToBeLogged(Level::warning, makeTag("nx::second::className2")));
    EXPECT_TRUE(logger.isToBeLogged(Level::debug, makeTag("nx::first::className3")));
    EXPECT_FALSE(logger.isToBeLogged(Level::debug, makeTag("nx::second::className4")));

    logger.log(Level::warning, makeTag("nx::first::className1"), "aaa");
    logger.log(Level::warning, makeTag("nx::second::className2"), "bbb");
    logger.log(Level::debug, makeTag("nx::first::className3"), "ccc");
    logger.log(Level::debug, makeTag("nx::second::className4"), "ddd");
    ASSERT_EQ((size_t) 3, buffer->takeMessages().size());

    logger.setLevelFilters(LevelFilters{
        {makeFilter("nx::second"), Level::verbose}, {makeFilter("nx::third"), Level::none}});
    ASSERT_EQ((size_t) 2, logger.levelFilters().size());

    EXPECT_TRUE(logger.isToBeLogged(Level::warning, makeTag("nx::first::className1")));
    EXPECT_TRUE(logger.isToBeLogged(Level::warning, makeTag("nx::second::className2")));
    EXPECT_FALSE(logger.isToBeLogged(Level::warning, makeTag("nx::third::className3")));
    EXPECT_FALSE(logger.isToBeLogged(Level::debug, makeTag("nx::first::className4")));
    EXPECT_TRUE(logger.isToBeLogged(Level::debug, makeTag("nx::second::className5")));
    EXPECT_FALSE(logger.isToBeLogged(Level::debug, makeTag("nx::third::className6")));

    logger.log(Level::warning, makeTag("nx::first::className1"), "aaa");
    logger.log(Level::warning, makeTag("nx::second::className2"), "bbb");
    logger.log(Level::warning, makeTag("nx::third::className3"), "ccc");
    logger.log(Level::debug, makeTag("nx::first::className4"), "ddd");
    logger.log(Level::debug, makeTag("nx::second::className5"), "eee");
    logger.log(Level::debug, makeTag("nx::third::className6"), "fff");
    EXPECT_EQ((size_t) 3, buffer->takeMessages().size());

    EXPECT_EQ(3, levelChangedCount);
}

TEST(LogLogger, Format)
{
    const auto buffer = new Buffer();
    int levelChangedCount = 0;
    const Logger::OnLevelChanged onLevelChanged = [&levelChangedCount](){ ++levelChangedCount; };

    Logger logger(
        std::set<Filter>(),
        Level::verbose,
        std::unique_ptr<AbstractWriter>(buffer));
    logger.setOnLevelChanged(onLevelChanged);
    EXPECT_EQ(0, levelChangedCount);

    logger.log(Level::verbose, Tag(), "Message without tag");
    logger.log(Level::error, makeTag("nx::bbb::Object(2)"), "Second message");
    logger.log(Level::warning, makeTag("nx::ccc::Object(3)"), "Third message");
    logger.log(Level::info, makeTag("nx::ddd::Object(4)"), "Forth message");
    logger.log(Level::debug, makeTag("nx::eee::Object(5)"), "Fifth message");
    logger.log(Level::verbose, makeTag("nx::fff::Object(6)"), "Sixth message");
    logger.log(Level::trace, makeTag("nx::fff::Object(7)"), "Seventh message"); //< Ignored.

    const auto messages = buffer->takeMessages();
    ASSERT_EQ(6U, messages.size());

    const auto today = QDateTime::currentDateTime().toString("yyyy-MM-dd");
    const auto verifyMessage =
        [&](size_t index, const QString& line)
        {
            const auto& message = messages[index];
            EXPECT_TRUE(message.startsWith(today)) << message.toStdString();
            EXPECT_TRUE(message.endsWith(line)) << message.toStdString();
        };

    verifyMessage(0, QLatin1String("VERBOSE : Message without tag"));
    verifyMessage(1, QLatin1String("ERROR nx::bbb::Object(2): Second message"));
    verifyMessage(2, QLatin1String("WARNING nx::ccc::Object(3): Third message"));
    verifyMessage(3, QLatin1String("INFO nx::ddd::Object(4): Forth message"));
    verifyMessage(4, QLatin1String("DEBUG nx::eee::Object(5): Fifth message"));
    verifyMessage(5, QLatin1String("VERBOSE nx::fff::Object(6): Sixth message"));

    EXPECT_EQ(0, levelChangedCount);
}

TEST(LogLogger, default_log_level_trace_downgraded_to_verbose)
{
    auto buffer = std::make_unique<Buffer>();
    int levelChangedCount = 0;
    const Logger::OnLevelChanged onLevelChanged = [&levelChangedCount](){ ++levelChangedCount; };

    Logger logger(
        std::set<Filter>(),
        Level::trace,
        std::move(buffer));
    logger.setOnLevelChanged(onLevelChanged);
    EXPECT_EQ(0, levelChangedCount);

    EXPECT_EQ(Level::verbose, logger.defaultLevel());

    logger.setDefaultLevel(Level::trace);

    EXPECT_EQ(1, levelChangedCount);

    EXPECT_EQ(Level::verbose, logger.defaultLevel());
}

TEST(LogLogger, log_level_trace_allowed_with_non_empty_level_filters)
{
    auto buf = std::make_unique<Buffer>();
    const auto buffer = buf.get();

    int levelChangedCount = 0;
    const Logger::OnLevelChanged onLevelChanged = [&levelChangedCount](){ ++levelChangedCount; };

    Logger logger(
        std::set<Filter>(),
        Level::trace,
        std::move(buf));
    logger.setOnLevelChanged(onLevelChanged);
    EXPECT_EQ(0, levelChangedCount);

    logger.setLevelFilters({{Filter(std::string("nx")), Level::trace}});

    logger.log(Level::trace, makeTag("nx::ggg::Object(1)"), "trace me");

    EXPECT_EQ(1, buffer->takeMessages().size());
}

TEST(LogLogger, log_level_trace_ignored_with_empty_level_filters)
{
    auto buf = std::make_unique<Buffer>();
    const auto buffer = buf.get();

    int levelChangedCount = 0;
    const Logger::OnLevelChanged onLevelChanged = [&levelChangedCount](){ ++levelChangedCount; };

    Logger logger(
        std::set<Filter>(),
        Level::trace,
        std::move(buf));
    logger.setOnLevelChanged(onLevelChanged);
    EXPECT_EQ(0, levelChangedCount);

    logger.log(Level::trace, Tag(), "trace me");

    EXPECT_EQ(0, buffer->takeMessages().size());
}

} // namespace nx::log::test
