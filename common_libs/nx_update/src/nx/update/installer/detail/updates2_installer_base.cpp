#include "updates2_installer_base.h"
#include <utils/update/zip_utils.h>
#include <nx/utils/raii_guard.h>
#include <nx/utils/log/log.h>

namespace nx {
namespace update {
namespace detail {

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
                {
                    auto checkResult = self->checkContents();
                    if (checkResult != PrepareResult::ok)
                        return handler(checkResult);

                    return handler(PrepareResult::ok);
                }
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

void Updates2InstallerBase::install()
{

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
