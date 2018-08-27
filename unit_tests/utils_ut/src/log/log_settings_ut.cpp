#include <gtest/gtest.h>

#include <nx/utils/deprecated_settings.h>
#include <nx/utils/log/log_initializer.h>
#include <nx/utils/log/log_settings.h>

namespace nx {
namespace utils {
namespace log {
namespace test {

class LogSettings:
    public ::testing::Test
{
protected:
    void parse(std::vector<const char*> args)
    {
        QnSettings settings("company", "app", "mod");
        settings.parseArgs((int) args.size(), args.data());

        logSettings.load(settings, "log");
    }

    log::Settings logSettings;

private:
};

TEST_F(LogSettings, correct_parsing)
{
    parse({
        "-log/logDir", "/var/log/",
        "-log/logLevel", "DEBUG2",
        "-log/maxBackupCount", "77",
        "-log/maxFileSize", "184632",
    });

    ASSERT_EQ(1U, logSettings.loggers.size());
    ASSERT_EQ(Level::verbose, logSettings.loggers.front().level.primary);
    ASSERT_EQ(QString("/var/log/"), logSettings.loggers.front().directory);
    ASSERT_EQ(77, logSettings.loggers.front().maxBackupCount);
    ASSERT_EQ(184632U, logSettings.loggers.front().maxFileSize);
}

TEST_F(LogSettings, multiple_loggers)
{
    parse({
        "-log/logger", "file=-,level=WARNING",
        "-log/logger",
        "dir=/var/log/,maxBackupCount=11,maxFileSize=100M,"
            "level=WARNING[nx::network,nx::utils],level=DEBUG[nx::network::http],level=none"
    });

    ASSERT_EQ(2U, logSettings.loggers.size());

    LoggerSettings logger0Settings;
    logger0Settings.level.primary = nx::utils::log::Level::warning;
    logger0Settings.logBaseName = "-";
    ASSERT_EQ(logger0Settings, logSettings.loggers[0]);

    LoggerSettings logger1Settings;
    logger1Settings.directory = "/var/log/";
    logger1Settings.maxBackupCount = 11;
    logger1Settings.maxFileSize = 100 * 1024 * 1024;
    logger1Settings.level.primary = nx::utils::log::Level::none;
    logger1Settings.level.filters[Tag(QString("nx::network"))] =
        nx::utils::log::Level::warning;
    logger1Settings.level.filters[Tag(QString("nx::utils"))] =
        nx::utils::log::Level::warning;
    logger1Settings.level.filters[Tag(QString("nx::network::http"))] =
        nx::utils::log::Level::debug;
    ASSERT_EQ(logger1Settings, logSettings.loggers[1]);
}

TEST_F(LogSettings, compatibility_settings)
{
    parse({
        "--log-level=verbose",
        "--log-file=/var/log/utils_ut"
    });

    ASSERT_EQ(1U, logSettings.loggers.size());

    LoggerSettings logger0Settings;
    logger0Settings.level.primary = nx::utils::log::Level::verbose;
    logger0Settings.logBaseName = "/var/log/utils_ut";
    ASSERT_EQ(logger0Settings, logSettings.loggers[0]);
}

} // namespace test
} // namespace log
} // namespace utils
} // namespace nx
