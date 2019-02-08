#include "server_update_installer.h"
#include <media_server/media_server_module.h>
#include <media_server/settings.h>
#include <media_server/serverutil.h>
#include <common/static_common_module.h>

namespace nx {
namespace vms::server {

ServerUpdateInstaller::ServerUpdateInstaller(QnMediaServerModule* serverModule):
    CommonUpdateInstaller(serverModule->commonModule()),
    ServerModuleAware(serverModule)
{
}

QString ServerUpdateInstaller::dataDirectoryPath() const
{
     return QDir::temp().absolutePath();
}

bool ServerUpdateInstaller::initializeUpdateLog(const QString& targetVersion, QString* logFileName) const
{
    QString logDir;
    if (settings().logDir.present())
        logDir = settings().logDir();
    else
        logDir = settings().dataDir() + lit("/log/");

    if (logDir.isEmpty())
    {
        NX_WARNING(this, lm("Failed to find a proper path for a log file for version %1 installer").arg(targetVersion));
        return false;
    }

    QString fileName = QDir(logDir).absoluteFilePath("update.log");
    QFile logFile(fileName);
    if (!logFile.open(QFile::Append))
    {
        NX_WARNING(this, lm("Failed to create log file %1 for version %2 installer").args(fileName, targetVersion));
        return false;
    }

    QByteArray preface;
    preface.append("================================================================================\n");
    preface.append(lit(" [%1] Starting system update:\n").arg(QDateTime::currentDateTime().toString()));
    preface.append(lit("    Current version: %1\n").arg(qnStaticCommon->engineVersion().toString()));
    preface.append(lit("    Target version: %1\n").arg(targetVersion));
    preface.append("================================================================================\n");

    logFile.write(preface);
    logFile.close();

    *logFileName = fileName;
    NX_INFO(this, lm("initializeUpdateLog() - created log file %1 for version %2 installer").args(fileName, targetVersion));
    return true;
}

} // namespace vms::server
} // namespace nx
