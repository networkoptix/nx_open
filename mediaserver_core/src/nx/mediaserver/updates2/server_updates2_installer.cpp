#include "server_updates2_installer.h"
#include <media_server/media_server_module.h>
#include <media_server/settings.h>
#include <media_server/serverutil.h>
#include <common/static_common_module.h>

namespace nx {
namespace mediaserver {
namespace updates2 {

namespace {

const QString kUpdateLogFileName = lit("update.log");

} // namespace

QString ServerUpdates2Installer::dataDirectoryPath() const
{
    return getDataDirectory();
}

bool ServerUpdates2Installer::initializeUpdateLog(
    const QString& targetVersion,
    QString* logFileName) const
{
    QString logDir;
    if (qnServerModule->settings().logDir.present())
        logDir = qnServerModule->settings().logDir();
    else
        logDir = getDataDirectory() + lit("/log/");

    if (logDir.isEmpty())
        return false;

    QString fileName = QDir(logDir).absoluteFilePath(kUpdateLogFileName);
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

} // namespace updates2
} // namespace mediaserver
} // namespace nx
