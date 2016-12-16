#pragma once

#include "log.h"
#include "../settings.h"

namespace nx {
namespace utils {
namespace log {

class NX_UTILS_API QnLogSettings
{
public:
#ifdef _DEBUG
    static constexpr QnLogLevel kDefaultLogLevel = cl_logDEBUG1;
#else
    static constexpr QnLogLevel kDefaultLogLevel = cl_logINFO;
#endif

    QnLogLevel level = kDefaultLogLevel;
    QString directory = QString(); //< dataDir/log
    quint32 maxFileSize = nx::utils::stringToBytesConst("10M");
    quint8 maxBackupCount = 5;

    /** Rewrites values from settings if specified */
    void load(const QnSettings& settings, const QString& prefix = QLatin1String("log"));
};

void NX_UTILS_API initializeQnLog(
    const QnLogSettings& settings,
    const QString& dataDir,
    const QString& applicationName,
    const QString& baseName = QLatin1String("log_file"),
    int id = QnLog::MAIN_LOG_ID);

} // namespace log
} // namespace utils
} // namespace nx
