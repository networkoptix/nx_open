// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <mutex>

#include <QtCore/QPointer>
#include <nx/vms/client/core/resource/resource_fwd.h>
#include <nx/vms/common/system_context_aware.h>

#include "local_resources_directory_model.h"

namespace nx::vms::client::desktop {

class UpdateFileLayoutHelper: public QObject, public nx::vms::common::SystemContextAware
{
    Q_OBJECT

public:
    UpdateFileLayoutHelper(
        nx::vms::common::SystemContext* systemContext,
        QObject* parent = nullptr);

    // These methods must be called from the main thread.
    Q_INVOKABLE void startUpdateLayout(const QnFileLayoutResourcePtr& sourceLayout);
    Q_INVOKABLE void finishUpdateLayout(const QnFileLayoutResourcePtr& loadedLayout);
};

class LocalResourceProducer: public QObject, public nx::vms::common::SystemContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    LocalResourceProducer(
        nx::vms::common::SystemContext* systemContext,
        UpdateFileLayoutHelper* helper,
        QObject* parent = nullptr);
    void createLocalResources(const QStringList& pathList);
    void updateFileLayoutResource(const QString& path);
    void updateVideoFileResource(const QString& path);

private:
    UpdateFileLayoutHelper* m_updateFileLayoutHelper;
};

class ResourceDirectoryBrowser: public QObject, public nx::vms::common::SystemContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    ResourceDirectoryBrowser(
        nx::vms::common::SystemContext* systemContext,
        QObject* parent = nullptr);
    virtual ~ResourceDirectoryBrowser() override;

    void stop();

    void setLocalResourcesDirectories(const QStringList& paths);

    static QnFileLayoutResourcePtr layoutFromFile(const QString& filename,
        const QPointer<QnResourcePool>& resourcePool);

    static QnResourcePtr createArchiveResource(const QString& filename,
        const QPointer<QnResourcePool>& resourcePool);

private:
    void dropResourcesFromDirectory(const QString& path);
    void stopInternal();

private:
    LocalResourcesDirectoryModel* m_localResourceDirectoryModel;
    QThread* m_resourceProducerThread;
    LocalResourceProducer* m_resourceProducer;
    std::once_flag m_stopOnceFlag;
};

} // namespace nx::vms::client::desktop
