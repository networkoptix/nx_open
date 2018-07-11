#include "updates_installer.h"

#include <client/client_settings.h>
#include <client/client_module.h>
#include <common/static_common_module.h>

namespace nx {
namespace client {
namespace desktop {

namespace {

QString getDataDirectory()
{
    return qApp->applicationDirPath();
    //return qnServerModule->settings().dataDir();
}

const QString kUpdateLogFileName = lit("update.log");

} // namespace

QString UpdatesInstaller::dataDirectoryPath() const
{
    return getDataDirectory();
}

bool UpdatesInstaller::initializeUpdateLog(
    const QString& targetVersion,
    QString* logFileName) const
{
    QString logDir;
    /*
    if (qnClientModule->settings().logDir.present())
        logDir = qnClientModule->settings().logDir();
    else*/
    logDir = getDataDirectory() + lit("/log/");

    if (logDir.isEmpty())
        return false;

    QString fileName = QDir(logDir).absoluteFilePath(kUpdateLogFileName);
    QFile logFile(fileName);
    if (!logFile.open(QFile::Append))
        return false;

    QByteArray preface;
    preface.append("================================================================================\n");
    preface.append(lit(" [%1] Starting client update:\n").arg(QDateTime::currentDateTime().toString()));
    preface.append(lit("    Current version: %1\n").arg(qnStaticCommon->engineVersion().toString()));
    preface.append(lit("    Target version: %1\n").arg(targetVersion));
    preface.append("================================================================================\n");

    logFile.write(preface);
    logFile.close();

    *logFileName = fileName;
    return true;
}

} // namespace desktop
} // namespace client
} // namespace nx
