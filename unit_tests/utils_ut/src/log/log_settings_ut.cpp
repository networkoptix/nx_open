#include <gtest/gtest.h>

#include <nx/utils/settings.h>
#include <nx/utils/log/log_initializer.h>
#include <nx/utils/log/log_settings.h>

namespace nx {
namespace utils {
namespace log {
namespace test {

TEST(Settings, correct_parsing)
{
    const char* args[] = {
        "-log/logDir", "/var/log/",
        "-log/logLevel", "DEBUG2",
        "-log/maxBackupCount", "77",
        "-log/maxFileSize", "184632",
    };

    QnSettings settings("company", "app", "mod");
    settings.parseArgs(sizeof(args) / sizeof(*args), args);

    log::Settings logSettings;
    logSettings.load(settings, "log");

    ASSERT_EQ(Level::verbose, logSettings.level.primary);
    ASSERT_EQ(QString("/var/log/"), logSettings.directory);
    ASSERT_EQ(77, logSettings.maxBackupCount);
    ASSERT_EQ(184632U, logSettings.maxFileSize);
}

} // namespace test
} // namespace log
} // namespace utils
} // namespace nx
