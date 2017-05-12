#include "log_settings.h"

#include <nx/utils/settings.h>

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

    const auto confLevel = levelFromString(settings.value(makeKey("logLevel")).toString());
    if (confLevel != cl_logUNKNOWN)
        level = confLevel;

    const auto filters = settings.value(makeKey("exceptionFilters"))
        .toString().splitRef(QChar(','));
    for (const auto& f: filters)
        exceptionFilers.insert(f.toString());

    directory = settings.value(makeKey("logDir")).toString();
    maxBackupCount = (quint8)settings.value(makeKey("maxBackupCount"), 5).toInt();
    maxFileSize = (quint32)nx::utils::stringToBytes(
        settings.value(makeKey("maxFileSize")).toString(), maxFileSize);
}

} // namespace log
} // namespace utils
} // namespace nx
