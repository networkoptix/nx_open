#include "log_settings.h"

#include <nx/utils/deprecated_settings.h>
#include <nx/utils/string.h>

namespace nx {
namespace utils {
namespace log {

Settings::Settings()
{
}

void Settings::load(const QnSettings& settings, const QString& prefix)
{
    const auto makeKey =
        [&prefix](const char* key) { return QString(lm("%1/%2").arg(prefix).arg(key)); };

    level.parse(settings.value(makeKey("logLevel")).toString());
    directory = settings.value(makeKey("logDir")).toString();
    maxBackupCount = (quint8) settings.value(makeKey("maxBackupCount"), 5).toInt();
    maxFileSize = (quint32) nx::utils::stringToBytes(
        settings.value(makeKey("maxFileSize")).toString(), maxFileSize);
    logBaseName = settings.value(makeKey("baseName")).toString();
}

void Settings::updateDirectoryIfEmpty(const QString dataDirectory)
{
    if (directory.isEmpty())
        directory = dataDirectory + "/log";
}

} // namespace log
} // namespace utils
} // namespace nx
