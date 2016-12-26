#pragma once

#include "log.h"
#include "../settings.h"

namespace nx {
namespace utils {
namespace log {

class NX_UTILS_API Settings
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

} // namespace log
} // namespace utils
} // namespace nx
