#include "log_initializer.h"

#include <QtCore/QDir>

#include "log_settings.h"
#include "../app_info.h"

namespace nx {
namespace utils {
namespace log {

void initialize(
    const Settings& settings,
    const QString& dataDir,
    const QString& applicationName,
    const QString& baseName,
    int id)
{
    if (settings.level == cl_logNONE)
        return;

    const auto logDir = settings.directory.isEmpty()
        ? (dataDir + QLatin1String("/log"))
        : settings.directory;

    QDir().mkpath(logDir);
    const QString& fileName = logDir + lit("/") + baseName;
    if (!QnLog::instance(id)->create(
            fileName, settings.maxFileSize, settings.maxBackupCount, settings.level))
    {
        std::cerr << "Failed to create log file " << fileName.toStdString() << std::endl;
    }

    const auto logInfo = lm("Logging level: %1, maxFileSize: %2, maxBackupCount: %3, fileName: %4")
        .strs(QnLog::logLevelToString(settings.level),
            nx::utils::bytesToString(settings.maxFileSize),
            settings.maxBackupCount, fileName);

    NX_LOG(id, lm(QByteArray(80, '=')), cl_logALWAYS);
    NX_LOG(id, lm("%1 started").arg(applicationName), cl_logALWAYS);
    NX_LOG(id, lm("Software version: %1").arg(AppInfo::applicationVersion()), cl_logALWAYS);
    NX_LOG(id, lm("Software revision: %1").arg(AppInfo::applicationRevision()), cl_logALWAYS);
    NX_LOG(id, logInfo, cl_logALWAYS);
}

} // namespace log
} // namespace utils
} // namespace nx
