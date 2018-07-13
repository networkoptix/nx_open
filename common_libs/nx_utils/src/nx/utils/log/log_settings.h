#pragma once

#include <vector>

#include "log_logger.h"

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
     * Str = param (; param)
     * param = key [= value]
     * value = TEXT
     * param = file | dir | maxBackupCount | maxFileSize | level
     * level = LogLevel [: messageTagPrefix]
     * file = - | fileName
     *
     * "-" meand STDOUT
     *
     * Examples:
     *   file=-;level=WARNING
     *   dir=/var/log/;maxBackupCount=11;maxFileSize=100M;
     *       level=WARNING:nx::network;level=DEBUG:nx::network::http;level=none
     */
    void parse(const QString& str);
    void updateDirectoryIfEmpty(const QString& dataDirectory);

    bool operator==(const LoggerSettings& right) const;

private:
    void loadLevel(const QString& str);
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
