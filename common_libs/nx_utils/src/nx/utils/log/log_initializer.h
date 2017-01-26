#pragma once

#include "log.h"
#include "../settings.h"

namespace nx {
namespace utils {
namespace log {

class Settings;

void NX_UTILS_API initialize(
    const Settings& settings,
    const QString& dataDir,
    const QString& applicationName,
    const QString& baseName = QLatin1String("log_file"),
    int id = QnLog::MAIN_LOG_ID);

} // namespace log
} // namespace utils
} // namespace nx
