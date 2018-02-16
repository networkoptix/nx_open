#include "updates2_installer_base.h"
#include <utils/update/zip_utils.h>
#include <utils/common/process.h>
#include <nx/utils/raii_guard.h>
#include <nx/utils/log/log.h>

namespace nx {
namespace update {
namespace detail {

//bool QnServerUpdateTool::initializeUpdateLog(const QString& targetVersion, QString* logFileName) const
//{
//    QString logDir = qnServerModule->roSettings()->value(lit("logDir"), getDataDirectory() + lit("/log/")).toString();
//    if (logDir.isEmpty())
//        return false;
//
//    QString fileName = QDir(logDir).absoluteFilePath(updateLogFileName);
//    QFile logFile(fileName);
//    if (!logFile.open(QFile::Append))
//        return false;
//
//    QByteArray preface;
//    preface.append("================================================================================\n");
//    preface.append(QString(lit(" [%1] Starting system update:\n")).arg(QDateTime::currentDateTime().toString()));
//    preface.append(QString(lit("    Current version: %1\n")).arg(qnStaticCommon->engineVersion().toString()));
//    preface.append(QString(lit("    Target version: %1\n")).arg(targetVersion));
//    preface.append("================================================================================\n");
//
//    logFile.write(preface);
//    logFile.close();
//
//    *logFileName = fileName;
//    return true;
//}

void Updates2InstallerBase::prepareAsync(const QString& path, PrepareUpdateCompletionHandler handler)
{
    {
        QnMutexLocker lock(&m_mutex);
        if (!m_extractor)
            m_extractor = createZipExtractor();

        if (m_running)
            return handler(PrepareResult::alreadyStarted);

        if (!cleanInstallerDirectory())
        {
            lock.unlock();
            return handler(PrepareResult::cleanTemporaryFilesError);
        }

        m_running = true;
    }

    m_extractor->extractAsync(
        path,
        installerWorkDir(),
        [self = this, handler](QnZipExtractor::Error errorCode)
        {
            auto cleanupGuard = QnRaiiGuard::createDestructible(
                [self, errorCode]()
                {
                    if (errorCode != QnZipExtractor::Error::Ok)
                        self->cleanInstallerDirectory();

                    QnMutexLocker lock(&self->m_mutex);
                    self->m_running = false;
                    self->m_condition.wakeOne();
                });

            switch (errorCode)
            {
                case QnZipExtractor::Error::Ok:
                    return handler(self->checkContents());
                case QnZipExtractor::Error::NoFreeSpace:
                    return handler(PrepareResult::noFreeSpace);
                default:
                    NX_WARNING(
                        self,
                        lm("ZipExtractor error: %1").args(QnZipExtractor::errorToString(errorCode)));
                    return handler(PrepareResult::unknownError);
            }
        });
}

PrepareResult Updates2InstallerBase::checkContents() const
{
    QVariantMap infoMap = updateInformation();
    if (infoMap.isEmpty())
        return PrepareResult::updateContentsError;

    QString executable = infoMap.value("executable").toString();
    if (executable.isEmpty())
    {
        NX_ERROR(this, "No executable specified in the update information file");
        return PrepareResult::updateContentsError;
    }

    if (!checkExecutable(executable))
    {
        NX_ERROR(this, "Update executable file is invalid");
        return PrepareResult::updateContentsError;
    }

    QnSystemInformation systemInfo = systemInformation();

    QString platform = infoMap.value("platform").toString();
    if (platform != systemInfo.platform)
    {
        NX_ERROR(this, lm("Incompatible update: %1 != %2").args(systemInfo.platform, platform));
        return PrepareResult::updateContentsError;
    }

    QString arch = infoMap.value("arch").toString();
    if (arch != systemInfo.arch)
    {
        NX_ERROR(this, lm("Incompatible update: %1 != %2").args(systemInfo.arch, arch));
        return PrepareResult::updateContentsError;
    }

    QString modification = infoMap.value("modification").toString();
    if (modification != systemInfo.modification)
    {
        NX_ERROR(this, lm("Incompatible update: %1 != %2").args(systemInfo.modification, modification));
        return PrepareResult::updateContentsError;
    }

    return PrepareResult::ok;
}

bool Updates2InstallerBase::install()
{

    // #TODO #akulikov Implement this

    //QString logFileName;
    //if (initializeUpdateLog(version, &logFileName))
    //    arguments.append(logFileName);
    //else
    //    NX_LOG("QnServerUpdateTool: Could not create or open update log file.", cl_logWARNING);

    //QFile executableFile(updateDir.absoluteFilePath(executable));
    //if (!executableFile.exists()) {
    //    NX_LOG(lit("QnServerUpdateTool: The specified executable doesn't exists: %1").arg(executable), cl_logERROR);
    //    return false;
    //}
    //if (!executableFile.permissions().testFlag(QFile::ExeOwner)) {
    //    NX_LOG(lit("QnServerUpdateTool: The specified executable doesn't have an execute permission: %1").arg(executable), cl_logWARNING);
    //    executableFile.setPermissions(executableFile.permissions() | QFile::ExeOwner);
    //}
    //if (nx::utils::log::mainLogger()->isToBeLogged(nx::utils::log::Level::debug))
    //{
    //    QString argumentsStr(" APPSERVER_PASSWORD=\"\" APPSERVER_PASSWORD_CONFIRM=\"\" SERVER_PASSWORD=\"\" SERVER_PASSWORD_CONFIRM=\"\"");
    //    for (const QString& arg : arguments)
    //        argumentsStr += lit(" ") + arg;

    //    NX_LOG(lit("QnServerUpdateTool: Launching %1 %2").arg(executable).arg(argumentsStr), cl_logINFO);
    //}

    //const SystemError::ErrorCode processStartErrorCode = nx::startProcessDetached(QDir(installerWorkDir()).absoluteFilePath(executable), arguments);
    //if (processStartErrorCode == SystemError::noError) {
    //    NX_LOG("QnServerUpdateTool: Update has been started.", cl_logINFO);
    //}
    //else {
    //    NX_LOG(lit("QnServerUpdateTool: Cannot launch update script. %1").arg(SystemError::toString(processStartErrorCode)), cl_logERROR);
    //}

    //QDir::setCurrent(currentDir);

    //return true;

    return false;
}

QString Updates2InstallerBase::installerWorkDir() const
{
    return dataDirectoryPath() + QDir::separator() + ".installer";
}

void Updates2InstallerBase::stopSync()
{
    QnMutexLocker lock(&m_mutex);
    while (m_running)
        m_condition.wait(lock.mutex());
}

Updates2InstallerBase::~Updates2InstallerBase()
{
    stopSync();
}

} // namespace detail
} // namespace update
} // namespace nx
