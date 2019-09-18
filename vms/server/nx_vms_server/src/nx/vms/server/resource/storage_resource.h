#pragma once

#include <atomic>
#include <memory>
#include <QtCore/QSharedPointer>
#include <core/resource/storage_resource.h>

class QnMediaServerModule;

namespace nx::vms::server {

class StorageResource: public QnStorageResource
{
public:
    StorageResource(QnMediaServerModule* serverModule);

    virtual qint64 nxOccupedSpace() const override;

    virtual QIODevice* open(
        const QString &fileName,
        QIODevice::OpenMode openMode) override final;

    struct Metrics
    {
        std::atomic<qint64> bytesRead{0};
        std::atomic<qint64> bytesWritten{0};
    };

    qint64 getAndResetMetric(std::atomic<qint64> Metrics::*parameter);

protected:
    QnMediaServerModule* serverModule() const { return m_serverModule; }
    QIODevice* wrapIoDevice(std::unique_ptr<QIODevice> ioDevice);
private:
    QnMediaServerModule* m_serverModule = nullptr;
    std::shared_ptr<Metrics> m_metrics;
};

using StorageResourcePtr = QnSharedResourcePointer<StorageResource>;
using StorageResourceList = QnSharedResourcePointerList<StorageResource>;

} // namespace nx::vms::server
