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

qint64 StorageResource::getAndResetMetric(std::atomic<qint64> Metrics::* parameter)
{
    return (*m_metrics.*parameter).exchange(0);
}

QIODevice* StorageResource::open(const QString &fileName, QIODevice::OpenMode openMode)
{
    auto sourceIoDevice = openInternal(fileName, openMode);
    return wrapIoDevice(std::unique_ptr<QIODevice>(sourceIoDevice));
}

QIODevice* StorageResource::wrapIoDevice(std::unique_ptr<QIODevice> ioDevice)
{
    auto result = new nx::utils::IoDeviceWrapper(std::move(ioDevice));

    std::weak_ptr<Metrics> statistics = m_metrics;
    result->setOnWriteCallback(
        [statistics](qint64 size)
        {
            if (auto strongRef = statistics.lock(); size > 0 && strongRef)
                strongRef->bytesWritten += size;
        });
    result->setOnReadCallback(
        [statistics](qint64 size)
        {
            if (auto strongRef = statistics.lock(); size > 0 && strongRef)
                strongRef->bytesRead += size;
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

} // namespace nx::vms::server
