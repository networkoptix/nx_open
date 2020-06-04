#include "storage_resource.h"
#include <media_server/media_server_module.h>
#include <recorder/storage_manager.h>
#include <nx/utils/iodevice_wrapper.h>
#include <recorder/file_deletor.h>

namespace nx::vms::server {

StorageResource::StorageResource(QnMediaServerModule* serverModule):
    QnStorageResource(serverModule->commonModule()),
    m_serverModule(serverModule),
    m_metrics(std::make_shared<Metrics>())
{
}

qint64 StorageResource::getMetric(std::atomic<qint64> Metrics::* parameter)
{
    return (*m_metrics.*parameter).load();
}

QIODevice* StorageResource::open(const QString &fileName, QIODevice::OpenMode openMode)
{
    auto sourceIoDevice = openInternal(fileName, openMode);
    if (!sourceIoDevice)
        return sourceIoDevice;
    return wrapIoDevice(std::unique_ptr<QIODevice>(sourceIoDevice));
}

QIODevice* StorageResource::wrapIoDevice(std::unique_ptr<QIODevice> ioDevice)
{
    auto result = new nx::utils::IoDeviceWrapper(std::move(ioDevice));

    std::weak_ptr<Metrics> statistics = m_metrics;
    result->setOnWriteCallback(
        [this, statistics](qint64 size, std::chrono::milliseconds elapsed)
        {
            if (auto strongRef = statistics.lock())
            {
                if (size > 0)
                    strongRef->bytesWritten += size;

                if (elapsed > serverModule()->settings().ioOperationTimeTresholdSec())
                    strongRef->timedOutWrites++;

                strongRef->writes++;
            }
        });

    result->setOnReadCallback(
        [this, statistics](qint64 size, std::chrono::milliseconds elapsed)
        {
            if (auto strongRef = statistics.lock())
            {
                if (size > 0)
                    strongRef->bytesRead += size;

                if (elapsed > serverModule()->settings().ioOperationTimeTresholdSec())
                    strongRef->timedOutReads++;

                strongRef->reads++;
            }
        });

    result->setOnSeekCallback(
        [this, statistics](qint64 result, std::chrono::milliseconds elapsed)
        {
            if (auto strongRef = statistics.lock())
            {
                strongRef->seeks++;
                if (elapsed > serverModule()->settings().ioOperationTimeTresholdSec())
                    strongRef->timedOutSeeks++;
            }
        });

    return result;
}

qint64 StorageResource::nxOccupedSpace() const
{
    const auto ptr = toSharedPointer().dynamicCast<StorageResource>();
    return m_serverModule->normalStorageManager()->nxOccupiedSpace(ptr) +
        m_serverModule->backupStorageManager()->nxOccupiedSpace(ptr) +
        m_serverModule->fileDeletor()->postponedFileSize(getId());
}

bool StorageResource::removeFile(const QString& url)
{
    m_metrics->deletions++;
    std::chrono::milliseconds elapsed = std::chrono::milliseconds(0);
    const bool result = nx::utils::measure(
        [this, &url]() { return doRemoveFile(url); }, &elapsed);
    if (elapsed > serverModule()->settings().ioOperationTimeTresholdSec())
        m_metrics->timedOutDeletions++;
    return result;
}

QnAbstractStorageResource::FileInfoList StorageResource::getFileList(
    const QString& url)
{
    m_metrics->directoryLists++;
    std::chrono::milliseconds elapsed = std::chrono::milliseconds(0);
    const auto result = nx::utils::measure(
        [this, &url]() { return doGetFileList(url); }, &elapsed);
    if (elapsed > serverModule()->settings().ioOperationTimeTresholdSec())
        m_metrics->timedOutDirectoryLists++;
    return result;
}

} // namespace nx::vms::server
