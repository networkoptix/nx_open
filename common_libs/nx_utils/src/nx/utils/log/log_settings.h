#pragma once

#include "log_logger.h"

class QnSettings;

namespace nx {
namespace utils {
namespace log {

class NX_UTILS_API Settings
{
public:
    LevelSettings level;
    QString directory = QString(); //< dataDir/log
    uint32_t maxFileSize = 10 * 1024 * 1024; //< 10 MB.
    uint8_t maxBackupCount = 5;
    QString logBaseName;

    Settings();

    /** Rewrites values from settings if specified. */
    void load(const QnSettings& settings, const QString& prefix = QLatin1String("log"));

    /** Updates directory if it's not specified from dataDirectory */
    void updateDirectoryIfEmpty(const QString dataDirectory);
};

} // namespace log
} // namespace utils
} // namespace nx
