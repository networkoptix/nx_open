#include "log_settings.h"

#include <QtCore/QDir>

#include "../app_info.h"

namespace nx {
namespace utils {
namespace log {

void Settings::load(const QnSettings& settings, const QString& prefix)
{
    const auto makeKey =
        [&prefix](const char* key)
        {
            return QString(lm("%1/%2").arg(prefix).arg(key));
        };

    const auto confLevel = 
        QnLog::logLevelFromString(settings.value(makeKey("logLevel")).toString());
    if (confLevel != cl_logUNKNOWN)
        level = confLevel;

    directory = settings.value(makeKey("logDir")).toString();
    maxBackupCount = (quint8)settings.value(makeKey("maxBackupCount"), 5).toInt();
    maxFileSize = (quint32)nx::utils::stringToBytes(
        settings.value(makeKey("maxFileSize")).toString(), maxFileSize);
}

} // namespace log
} // namespace utils
} // namespace nx
