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

    virtual bool removeFile(const QString& url) override final;

    struct Metrics
    {
        std::atomic<qint64> bytesRead{0};
        std::atomic<qint64> bytesWritten{0};
        std::atomic<qint64> issues{0};
        std::atomic<qint64> deletetions{0};
    };

    qint64 getMetric(std::atomic<qint64> Metrics::* parameter);

    void atStorageFailure() { m_metrics->issues++; }
    QnMediaServerModule* serverModule() const { return m_serverModule; }

protected:
    QIODevice* wrapIoDevice(std::unique_ptr<QIODevice> ioDevice);

private:
    QnMediaServerModule* m_serverModule = nullptr;
    std::shared_ptr<Metrics> m_metrics;

    virtual bool doRemoveFile(const QString& url) = 0;
};

using StorageResourcePtr = QnSharedResourcePointer<StorageResource>;
using StorageResourceList = QnSharedResourcePointerList<StorageResource>;

} // namespace nx::vms::server
