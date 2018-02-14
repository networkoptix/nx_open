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
                return handler(PrepareResult::ok);
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

void Updates2InstallerBase::install()
{

}

QString Updates2InstallerBase::installerWorkDir() const
{
    return dataDirectoryPath() + QDir::separator() + ".installer";
}

void Updates2InstallerBase::stop()
{
    QnMutexLocker lock(&m_mutex);
    while (m_running)
        m_condition.wait(lock.mutex());
}

Updates2InstallerBase::~Updates2InstallerBase()
{
    stop();
}

} // namespace detail
} // namespace update
} // namespace nx
