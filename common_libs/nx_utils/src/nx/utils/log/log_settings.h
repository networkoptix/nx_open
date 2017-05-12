#pragma once

#include <nx/utils/string.h>

#include "log_logger.h"

class QnSettings;

namespace nx {
namespace utils {
namespace log {

class NX_UTILS_API Settings
{
public:
    #ifdef _DEBUG
        static constexpr Level kDefaultLevel = Level::debug;
    #else
        static constexpr Level kDefaultLevel = Level::info;
    #endif

    Level level = kDefaultLevel;
    std::set<QString> exceptionFilers;
    QString directory = QString(); //< dataDir/log
    quint32 maxFileSize = nx::utils::stringToBytesConst("10M");
    quint8 maxBackupCount = 5;

    /** Rewrites values from settings if specified */
    void load(const QnSettings& settings, const QString& prefix = QLatin1String("log"));
};

} // namespace log
} // namespace utils
} // namespace nx
