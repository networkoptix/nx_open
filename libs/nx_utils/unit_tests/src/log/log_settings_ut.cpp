// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <QtCore/QDir>

#include <gtest/gtest.h>

#include <nx/utils/deprecated_settings.h>
#include <nx/utils/log/log_initializer.h>
#include <nx/utils/log/log_settings.h>
#include <nx/utils/test_support/test_options.h>

namespace nx::log::test {

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

    void constructFromIni(const QByteArray& configData)
    {
        QDir dir(nx::TestOptions::temporaryDirectoryPath());
        QDir().mkpath(dir.absolutePath());

        const auto fileName = dir.absolutePath() + "/logs.conf";
        {
            QFile file(fileName);
            ASSERT_TRUE(file.open(QFile::WriteOnly)) << fileName.toStdString() << "\n" << file.errorString().toStdString();
            file.write(configData);
        }

        QSettings qSettings(fileName, QSettings::IniFormat);
        logSettings = log::Settings(&qSettings);
    }

    log::Settings logSettings;
};

TEST_F(LogSettings, correct_parsing)
{
    parse({
        "-log/logDir", "/var/log/",
        "-log/logLevel", "DEBUG2",
        "-log/maxLogVolumeSizeB", "200000",
        "-log/maxLogFileSizeB", "184632",
    });

    ASSERT_EQ(1U, logSettings.loggers.size());
    ASSERT_EQ(Level::verbose, logSettings.loggers.front().level.primary);
    ASSERT_EQ(QString("/var/log/"), logSettings.loggers.front().directory);
    ASSERT_EQ(200000, logSettings.loggers.front().maxVolumeSizeB);
    ASSERT_EQ(184632U, logSettings.loggers.front().maxFileSizeB);
}

TEST_F(LogSettings, multiple_loggers)
{
    parse({
        "-log/logger", "file=-,level=WARNING",
        "-log/logger",
        "dir=/var/log/,maxLogVolumeSizeB=110M,maxLogFileSizeB=100M,"
            "level=WARNING[nx::network,nx::utils],level=DEBUG[nx::network::http],level=none"
    });

    ASSERT_EQ(2U, logSettings.loggers.size());

    LoggerSettings logger0Settings;
    logger0Settings.level.primary = nx::log::Level::warning;
    logger0Settings.logBaseName = "-";
    ASSERT_EQ(logger0Settings, logSettings.loggers[0]);

    LoggerSettings logger1Settings;
    logger1Settings.directory = "/var/log/";
    logger1Settings.maxVolumeSizeB = 110 * 1024 * 1024;
    logger1Settings.maxFileSizeB = 100 * 1024 * 1024;
    logger1Settings.level.primary = nx::log::Level::none;
    logger1Settings.level.filters[Filter(QString("nx::network"))] =
        nx::log::Level::warning;
    logger1Settings.level.filters[Filter(QString("nx::utils"))] =
        nx::log::Level::warning;
    logger1Settings.level.filters[Filter(QString("nx::network::http"))] =
        nx::log::Level::debug;
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
    logger0Settings.level.primary = nx::log::Level::verbose;
    logger0Settings.logBaseName = "/var/log/utils_ut";
    ASSERT_EQ(logger0Settings, logSettings.loggers[0]);
}

TEST_F(LogSettings, IniBase)
{
    constructFromIni(R"ini(
[General]
maxLogVolumeSizeB=1000000
maxLogFileSizeB=1000

[-]
debug=*
none=re:network

[client_log]
verbose=*
debug=re:^nx::vms::discovery
    )ini");

    ASSERT_EQ(2, logSettings.loggers.size());
    {
        auto logger = logSettings.loggers[0];
        EXPECT_EQ(logger.logBaseName, "-");
        EXPECT_EQ(logger.maxVolumeSizeB, 1000000);
        EXPECT_EQ(logger.maxFileSizeB, 1000);
        EXPECT_EQ(logger.maxFileTimePeriodS.count(), kDefaultMaxLogFileTimePeriodS.count());
        EXPECT_EQ(logger.level.primary, log::Level::debug);
        EXPECT_EQ(logger.level.filters.size(), 1);
        EXPECT_EQ(logger.level.filters[Filter(QString("re:network"))], log::Level::none);
    }
    {
        auto logger = logSettings.loggers[1];
        EXPECT_EQ(logger.logBaseName, "client_log");
        EXPECT_EQ(logger.maxVolumeSizeB, 1000000);
        EXPECT_EQ(logger.maxFileSizeB, 1000);
        EXPECT_EQ(logger.maxFileTimePeriodS.count(), kDefaultMaxLogFileTimePeriodS.count());
        EXPECT_EQ(logger.level.primary, log::Level::verbose);
        EXPECT_EQ(logger.level.filters.size(), 1);
        EXPECT_EQ(logger.level.filters[Filter(QString("re:^nx::vms::discovery"))], log::Level::debug);
    }
}

TEST_F(LogSettings, IniSectionOverride)
{
    constructFromIni(R"ini(
[General]
maxLogVolumeSizeB=1000000
maxLogFileSizeB=1000

[-]
maxLogVolumeSizeB=2000000
maxLogFileTimePeriodS=100500
debug=*
none=re:network

[client_log]
verbose=*
debug=re:^nx::vms::discovery
    )ini");

    ASSERT_EQ(2, logSettings.loggers.size());
    {
        auto logger = logSettings.loggers[0];
        EXPECT_EQ(logger.logBaseName, "-");
        EXPECT_EQ(logger.maxVolumeSizeB, 2000000);
        EXPECT_EQ(logger.maxFileSizeB, 1000);
        EXPECT_EQ(logger.maxFileTimePeriodS.count(), 100500);
        EXPECT_EQ(logger.level.primary, log::Level::debug);
        EXPECT_EQ(logger.level.filters.size(), 1);
        EXPECT_EQ(logger.level.filters[Filter(QString("re:network"))], log::Level::none);
    }
    {
        auto logger = logSettings.loggers[1];
        EXPECT_EQ(logger.logBaseName, "client_log");
        EXPECT_EQ(logger.maxVolumeSizeB, 1000000);
        EXPECT_EQ(logger.maxFileSizeB, 1000);
        EXPECT_EQ(logger.maxFileTimePeriodS.count(), kDefaultMaxLogFileTimePeriodS.count());
        EXPECT_EQ(logger.level.primary, log::Level::verbose);
        EXPECT_EQ(logger.level.filters.size(), 1);
        EXPECT_EQ(logger.level.filters[Filter(QString("re:^nx::vms::discovery"))], log::Level::debug);
    }
}

} // namespace nx::log::test
