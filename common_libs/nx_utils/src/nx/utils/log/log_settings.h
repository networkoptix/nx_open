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

    Level defaultLevel = kDefaultLevel;
    LevelFilters levelFilters;
    QString directory = QString(); //< dataDir/log
    uint32_t maxFileSize = (uint32_t) stringToBytesConst("10M");
    uint8_t maxBackupCount = 5;

    /** Rewrites values from settings if specified. */
    void load(const QnSettings& settings, const QString& prefix = QLatin1String("log"));

    /** Rewrites default level if is is possible to parse. */
    bool loadDefaultLevel(const QString& level);

    template<typename ... Levels>
    bool loadDefaultLevel(const QString& level, Levels ... levels)
    {
        return loadDefaultLevel(level) || loadDefaultLevel(std::forward<Levels>(levels) ...);
    }
};

} // namespace log
} // namespace utils
} // namespace nx
