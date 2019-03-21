#pragma once

#include "core/resource_management/resource_searcher.h"
#include <core/resource/client_resource_fwd.h>
#include "local_resources_directory_model.h"

namespace nx::vms::client::desktop {

class QnLocalResourceProducer: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    QnLocalResourceProducer(QObject* parent = nullptr);
    void createLocalResources(const QStringList& pathList);
};

class QnResourceDirectoryBrowser:
    public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    QnResourceDirectoryBrowser(QObject* parent = nullptr);
    virtual ~QnResourceDirectoryBrowser() override;

    void setLocalResourcesDirectories(const QStringList& paths);

    static QnFileLayoutResourcePtr layoutFromFile(const QString& filename,
        QnResourcePool* resourcePool);
    static QnResourcePtr resourceFromFile(const QString& filename, QnResourcePool* resourcePool);
    static QnResourcePtr createArchiveResource(const QString& filename,
        QnResourcePool* resourcePool);

signals:
    void initLocalResources(const QStringList& pathList);

private:
    void dropResourcesFromDirectory(const QString& path);

private:
    QnLocalResourcesDirectoryModel* m_localResourceDirectoryModel;
    QThread* m_resourceProducerThread;
    QnLocalResourceProducer* m_resourceProducer;
};

} // namespace nx::vms::client::desktop
