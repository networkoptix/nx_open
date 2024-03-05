// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include "log_level.h"
#include "log_writers.h"

class QSettings;
class QnSettings;

namespace nx::log {

static constexpr char kMaxLogVolumeSizeSymbolicName[] = "maxLogVolumeSizeB";
static constexpr char kMaxLogFileSizeSymbolicName[] = "maxLogFileSizeB";
static constexpr char kMaxLogFileTimePeriodSymbolicName[] = "maxLogFileTimePeriodS";
static constexpr char kLogArchivingEnabledSymbolicName[] = "logArchivingEnabled";

/**
 * Specifies configuration of a logger.
 *
 * Parses text of the following ABNF syntax:
 * <pre><code>
 * LOGGER_SETTINGS = param (";" param)*
 * param = key [= value]
 * value = TEXT
 * param = file | dir | maxLogVolumeSizeB | maxLogFileSizeB | maxLogFileTimePeriodS | level
 * level = LogLevel ["[" messageTagPrefixes "]"]
 * messageTagPrefixes = messageTagPrefix (", " messageTagPrefix)*
 * file = - | fileName
 * </code></pre>
 *
 * "-" means STDOUT
 *
 * For most applications logger settings are specified with `--log/logger=LOGGER_SETTINGS` argument.
 * There can be multiple `--log/logger=` arguments.
 *
 * LOGGER_SETTINGS examples:
 *
 * Log everything <= WARNING to stdout:
 * <pre><code>
 * file=-;level=WARNING
 * </code></pre>
 *
 * Log only nx::network::http* with <= VERBOSE level to /var/log/http_log file:
 * <pre><code>
 * dir=/var/log/;file=http_log;level=VERBOSE[nx::network::http];level=none
 * </code></pre>
 *
 * Log nx::network* with <= WARNING level and nx::network::http* with <= DEBUG level to a file:
 * <pre><code>
 * dir=/var/log/;level=WARNING[nx::network];level=DEBUG[nx::network::http];level=none;maxLogVolumeSizeB=110M;maxLogFileSizeB=100M
 * </code></pre>
 *
 * Generally, `level=none` means "ignore everything else".
 */
class NX_UTILS_API LoggerSettings
{
public:
    LevelSettings level;
    QString directory = QString(); //< dataDir/log
    qint64 maxVolumeSizeB = kDefaultMaxLogVolumeSizeB; //< 500 MB.
    qint64 maxFileSizeB = kDefaultMaxLogFileSizeB; //< 10 MB.
    std::chrono::seconds maxFileTimePeriodS = kDefaultMaxLogFileTimePeriodS; //< 0.
    bool archivingEnabled = kDefaultLogArchivingEnabled;
    QString logBaseName;

    bool parse(const QString& str);
    void updateDirectoryIfEmpty(const QString& logDirectory);

    bool operator==(const LoggerSettings& right) const;
};

class NX_UTILS_API Settings
{
public:
    std::vector<LoggerSettings> loggers;

    Settings() = default;
    explicit Settings(QSettings* settings);

    /** Rewrites values from settings if specified. */
    void load(const QnSettings& settings, const QString& prefix = QLatin1String("log"));

    /** Updates directory if it's not specified from dataDirectory */
    void updateDirectoryIfEmpty(const QString& logDirectory);

private:
    void loadCompatibilityLogger(
        const QnSettings& settings,
        const QString& prefix);
};

} // namespace nx::log
