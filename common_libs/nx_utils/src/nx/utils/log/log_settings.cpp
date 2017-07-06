#include "log_settings.h"

#include <nx/utils/settings.h>

namespace nx {
namespace utils {
namespace log {

bool Settings::loadDefaultLevel(const QString& level)
{
    const auto parsedLevel = levelFromString(level);
    if (parsedLevel == Level::undefined)
        return false;

    defaultLevel = parsedLevel;
    return true;
}

void Settings::load(const QnSettings& settings, const QString& prefix)
{
    const auto makeKey =
        [&prefix](const char* key) { return QString(lm("%1/%2").arg(prefix).arg(key)); };

    loadDefaultLevel({settings.value(makeKey("logLevel")).toString()});
    levelFilters = levelFiltersFromString(settings.value(makeKey("levelFilters")).toString());
    directory = settings.value(makeKey("logDir")).toString();
    maxBackupCount = (quint8) settings.value(makeKey("maxBackupCount"), 5).toInt();
    maxFileSize = (quint32) nx::utils::stringToBytes(
        settings.value(makeKey("maxFileSize")).toString(), maxFileSize);
}

} // namespace log
} // namespace utils
} // namespace nx
