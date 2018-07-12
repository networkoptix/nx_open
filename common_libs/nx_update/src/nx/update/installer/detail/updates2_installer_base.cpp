#include "updates2_installer_base.h"
#include <utils/update/zip_utils.h>
#include <utils/common/process.h>
#include <nx/utils/raii_guard.h>
#include <nx/utils/log/log.h>

namespace nx {
namespace update {
namespace installer {
namespace detail {

void Updates2InstallerBase::prepareAsync(const QString& path, PrepareUpdateCompletionHandler handler)
{
    {
        QnMutexLocker lock(&m_mutex);
        if (!m_extractor)
            m_extractor = createZipExtractor();

        NX_ASSERT(handler, "PrepareUpdateCompletionHandler should not be NULL");
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
        [self = this, handler](QnZipExtractor::Error errorCode, const QString& outputPath)
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
                    return handler(self->checkContents(outputPath));
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

PrepareResult Updates2InstallerBase::checkContents(const QString& outputPath) const
{
    QVariantMap infoMap = updateInformation(outputPath);
    if (infoMap.isEmpty())
        return PrepareResult::updateContentsError;

    if (infoMap.value("executable").toString().isEmpty())
    {
        NX_ERROR(this, "No executable specified in the update information file");
        return PrepareResult::updateContentsError;
    }

    m_executable = outputPath + QDir::separator() + infoMap.value("executable").toString();
    if (!checkExecutable(m_executable))
    {
        NX_ERROR(this, "Update executable file is invalid");
        return PrepareResult::updateContentsError;
    }

    const auto systemInfo = systemInformation();

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

    m_version = infoMap.value(QStringLiteral("version")).toString();

    return PrepareResult::ok;
}

bool Updates2InstallerBase::install()
{
    QString currentDir = QDir::currentPath();
    QDir::setCurrent(installerWorkDir());

    QStringList arguments;
    QString logFileName;

    if (initializeUpdateLog(m_version, &logFileName))
        arguments.append(logFileName);
    else
        NX_WARNING(this, "Failed to create or open update log file.");

    if (nx::utils::log::mainLogger()->isToBeLogged(nx::utils::log::Level::debug))
    {
        QString argumentsStr(
            " APPSERVER_PASSWORD=\"\" APPSERVER_PASSWORD_CONFIRM=\"\" SERVER_PASSWORD=\"\"\
             SERVER_PASSWORD_CONFIRM=\"\"");
        for (const QString& arg: arguments)
            argumentsStr += " " + arg;

        NX_INFO(this, lm("Launching %1 %2").arg(m_executable).args(argumentsStr));
    }

    const SystemError::ErrorCode processStartErrorCode = nx::startProcessDetached(
        QDir(installerWorkDir()).absoluteFilePath(m_executable),
        arguments);
    if (processStartErrorCode == SystemError::noError)
    {
        NX_INFO(this, "Update has been started.");
    }
    else
    {
        NX_ERROR(
            this,
            lm("Failed to launch update script. %1")
                .args(SystemError::toString(processStartErrorCode)));
    }

    QDir::setCurrent(currentDir);

    return true;
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
} // namespace installer
} // namespace update
} // namespace nx
