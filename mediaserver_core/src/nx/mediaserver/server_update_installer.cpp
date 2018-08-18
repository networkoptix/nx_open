#include "server_update_installer.h"
#include <media_server/media_server_module.h>
#include <media_server/settings.h>
#include <media_server/serverutil.h>
#include <common/static_common_module.h>

namespace nx {
namespace mediaserver {

ServerUpdateInstaller::ServerUpdateInstaller(QnMediaServerModule* serverModule):
    CommonUpdateInstaller(serverModule->commonModule()),
    ServerModuleAware(serverModule)
{ 
}

QString ServerUpdateInstaller::dataDirectoryPath() const
{
     return settings().dataDir();
}

bool ServerUpdateInstaller::initializeUpdateLog(const QString& targetVersion, QString* logFileName) const
{
    QString logDir;
    if (settings().logDir.present())
        logDir = settings().logDir();
    else
        logDir = settings().dataDir() + lit("/log/");

    if (logDir.isEmpty())
        return false;

    QString fileName = QDir(logDir).absoluteFilePath("update.log");
    QFile logFile(fileName);
    if (!logFile.open(QFile::Append))
        return false;

    QByteArray preface;
    preface.append("================================================================================\n");
    preface.append(lit(" [%1] Starting system update:\n").arg(QDateTime::currentDateTime().toString()));
    preface.append(lit("    Current version: %1\n").arg(qnStaticCommon->engineVersion().toString()));
    preface.append(lit("    Target version: %1\n").arg(targetVersion));
    preface.append("================================================================================\n");

    logFile.write(preface);
    logFile.close();

    *logFileName = fileName;
    return true;
}

} // namespace mediaserver
} // namespace nx