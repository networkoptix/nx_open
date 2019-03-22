#pragma once

#include <core/resource/client_resource_fwd.h>
#include "local_resources_directory_model.h"

namespace nx::vms::client::desktop {

class LocalResourceProducer: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    LocalResourceProducer(QObject* parent = nullptr);
    void createLocalResources(const QStringList& pathList);
};

class ResourceDirectoryBrowser:
    public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    ResourceDirectoryBrowser(QObject* parent = nullptr);
    virtual ~ResourceDirectoryBrowser() override;

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
    LocalResourcesDirectoryModel* m_localResourceDirectoryModel;
    QThread* m_resourceProducerThread;
    LocalResourceProducer* m_resourceProducer;
};

} // namespace nx::vms::client::desktop
