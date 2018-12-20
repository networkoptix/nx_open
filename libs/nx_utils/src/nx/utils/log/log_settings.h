#pragma once

#include <vector>

#include "log_level.h"

class QnSettings;

namespace nx {
namespace utils {
namespace log {

class NX_UTILS_API LoggerSettings
{
public:
    LevelSettings level;
    QString directory = QString(); //< dataDir/log
    uint64_t maxFileSize = 10 * 1024 * 1024; //< 10 MB.
    uint8_t maxBackupCount = 5;
    QString logBaseName;

    LoggerSettings() = default;

    /**
     * LOGGER_SETTINGS = param (";" param)*
     * param = key [= value]
     * value = TEXT
     * param = file | dir | maxBackupCount | maxFileSize | level
     * level = LogLevel ["[" messageTagPrefixes "]"]
     * messageTagPrefixes = messageTagPrefix (", " messageTagPrefix)*
     * file = - | fileName
     *
     * "-" means STDOUT
     *
     * For most applications logger settings are specified with --log/logger=LOGGER_SETTINGS argument.
     * There can be multiple --log/logger= arguments.
     *
     * LOGGER_SETTINGS examples:
     *
     * Log everything <= WARNING to stdout:
     *   file=-;level=WARNING
     *
     * Log only nx::network::http* with <= VERBOSE level to /var/log/http_log file:
     *   dir=/var/log/;file=http_log;level=VERBOSE:nx::network::http;level=none
     *
     * Log nx::network* with <= WARNING level and nx::network::http* with <= DEBUG level to a file:
     *   dir=/var/log/;level=WARNING:nx::network;level=DEBUG:nx::network::http;level=none;
     *       maxBackupCount=11;maxFileSize=100M;
     *
     * Generally, "level=none" means "ignore everything else".
     */
    void parse(const QString& str);
    void updateDirectoryIfEmpty(const QString& dataDirectory);

    bool operator==(const LoggerSettings& right) const;
};

class NX_UTILS_API Settings
{
public:
    std::vector<LoggerSettings> loggers;

    Settings() = default;

    /** Rewrites values from settings if specified. */
    void load(const QnSettings& settings, const QString& prefix = QLatin1String("log"));

    /** Updates directory if it's not specified from dataDirectory */
    void updateDirectoryIfEmpty(const QString& dataDirectory);

private:
    void loadCompatibilityLogger(
        const QnSettings& settings,
        const QString& prefix);
};

} // namespace log
} // namespace utils
} // namespace nx
