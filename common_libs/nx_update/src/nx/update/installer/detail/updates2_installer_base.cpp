#include "updates2_installer_base.h"
#include <utils/update/zip_utils.h>
#include <nx/utils/raii_guard.h>

namespace nx {
namespace update {
namespace detail {

void Updates2InstallerBase::prepareAsync(const QString& path, PrepareUpdateCompletionHandler handler)
{
    if (!cleanInstallerDirectory())
        return handler(PrepareResult::cleanTemporaryFilesError);

    AbstractZipExtractorPtr zipExtractor;
    {
        QnMutexLocker lock(&m_mutex);
        if (m_extractor)
            return handler(PrepareResult::alreadyStarted);

        m_extractor = createZipExtractor();
        zipExtractor = m_extractor;
    }

    zipExtractor->extractAsync(
        path,
        installerWorkDir(),
        [self = this, handler](QnZipExtractor::Error errorCode)
        {
            auto cleanupGuard = QnRaiiGuard::createDestructible(
                [self]()
                {
                    self->cleanInstallerDirectory();
                    QnMutexLocker lock(&self->m_mutex);
                    self->m_extractor.reset();
                    self->m_condition.wakeOne();
                });

            switch (errorCode)
            {
            case QnZipExtractor::Error::Ok:
                return handler(PrepareResult::ok);
            case QnZipExtractor::Error::NoFreeSpace:
                return handler(PrepareResult::noFreeSpace);
            default:
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
    if (m_extractor)
        m_extractor->stop();

    while (m_extractor)
        m_condition.wait(lock.mutex());
}

Updates2InstallerBase::~Updates2InstallerBase()
{
    stop();
}

} // namespace detail
} // namespace update
} // namespace nx
