// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_directory_browser.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QThread>

#include <common/common_module.h>

#include <core/resource/avi/avi_resource.h>
#include <core/resource/avi/filetypesupport.h>
#include <core/resource/layout_reader.h>
#include <core/resource/file_layout_resource.h>
#include <core/resource_management/resource_pool.h>
#include <client_core/client_core_module.h>
#include <nx/core/layout/layout_file_info.h>
#include <nx/vms/client/desktop/resources/layout_password_management.h>

namespace nx::vms::client::desktop {

UpdateFileLayoutHelper::UpdateFileLayoutHelper(QObject* parent):
    QObject(parent)
{
}

void UpdateFileLayoutHelper::startUpdateLayout(const QnFileLayoutResourcePtr& sourceLayout)
{
    NX_ASSERT(QCoreApplication::instance()->thread() == thread(),
        "Must be called from the main thread");

    const auto resources = sourceLayout->layoutResources();
    sourceLayout->setItems(QnLayoutItemDataList());
    for (const auto& resource: resources)
    {
        if (resource.dynamicCast<QnAviResource>())
            sourceLayout->resourcePool()->removeResource(resource);
    }
}

void UpdateFileLayoutHelper::finishUpdateLayout(const QnFileLayoutResourcePtr& loadedLayout)
{
    NX_ASSERT(QCoreApplication::instance()->thread() == thread(),
        "Must be called from the main thread");

    QnResourcePool* resourcePool = qnClientCoreModule->commonModule()->resourcePool();
    auto sourceLayout =
        resourcePool->getResourceByUrl(loadedLayout->getUrl()).dynamicCast<QnFileLayoutResource>();

    if (loadedLayout && sourceLayout)
    {
        loadedLayout->cloneItems(loadedLayout); //< This will alter item.id for items to prevent errors.
        sourceLayout->update(loadedLayout);
    }
}


ResourceDirectoryBrowser::ResourceDirectoryBrowser(QObject* parent):
    QObject(parent),
    m_localResourceDirectoryModel(new LocalResourcesDirectoryModel(this)),
    m_resourceProducerThread(new QThread(this)),
    m_resourceProducer(new LocalResourceProducer(new UpdateFileLayoutHelper(this)))
{
    m_resourceProducer->moveToThread(m_resourceProducerThread);

    connect(m_localResourceDirectoryModel, &LocalResourcesDirectoryModel::filesAdded,
        m_resourceProducer, &LocalResourceProducer::createLocalResources);

    connect(m_localResourceDirectoryModel, &LocalResourcesDirectoryModel::layoutFileChanged,
        m_resourceProducer, &LocalResourceProducer::updateFileLayoutResource);

    connect(m_localResourceDirectoryModel, &LocalResourcesDirectoryModel::videoFileChanged,
        m_resourceProducer, &LocalResourceProducer::updateVideoFileResource);

    connect(m_resourceProducerThread, &QThread::finished,
        m_resourceProducer, &LocalResourceProducer::deleteLater);

    m_resourceProducerThread->start(QThread::IdlePriority);
}

ResourceDirectoryBrowser::~ResourceDirectoryBrowser()
{
    stop();
}

void ResourceDirectoryBrowser::stop()
{
    std::call_once(m_stopOnceFlag, &ResourceDirectoryBrowser::stopInternal, this);
}

void ResourceDirectoryBrowser::setLocalResourcesDirectories(const QStringList& paths)
{
    QStringList cleanedPaths;
    for (const auto& path: paths)
    {
        const auto canonicalPath = QDir(path).canonicalPath();
        if (!canonicalPath.isEmpty())
            cleanedPaths.append(canonicalPath);
    }

    QStringList newDirectories = cleanedPaths.toSet().subtract(
        m_localResourceDirectoryModel->getLocalResourcesDirectories().toSet()).toList();

    QStringList removedDirectories = m_localResourceDirectoryModel->getLocalResourcesDirectories()
        .toSet().subtract(cleanedPaths.toSet()).toList();

    m_localResourceDirectoryModel->setLocalResourcesDirectories(cleanedPaths);

    for (const auto& removedDirectory: removedDirectories)
        dropResourcesFromDirectory(removedDirectory);
}

QnFileLayoutResourcePtr ResourceDirectoryBrowser::layoutFromFile(
    const QString& filename,
    const QPointer<QnResourcePool>& resourcePool)
{
    const QString layoutUrl = QDir::fromNativeSeparators(filename);
    NX_ASSERT(layoutUrl == filename);

    // Check the file still seems to be valid.
    const auto fileInfo = nx::core::layout::identifyFile(layoutUrl);
    if (!fileInfo.isValid)
        return QnFileLayoutResourcePtr();

    // Check that the layout does not exist yet.
    QnUuid layoutId = QnUuid::fromArbitraryData(layoutUrl);
    if (resourcePool)
    {
        auto existingLayout = resourcePool->getResourceById<QnFileLayoutResource>(layoutId);
        if (existingLayout)
            return existingLayout;
    }

    // Read layout from file.
    if (resourcePool)
        return layout::layoutFromFile(layoutUrl);

    return QnFileLayoutResourcePtr();
}

QnResourcePtr ResourceDirectoryBrowser::resourceFromFile(
    const QString& filename,
    const QPointer<QnResourcePool>& resourcePool)
{
    const QString path = QDir::fromNativeSeparators(filename);
    NX_ASSERT(path == filename);

    if (resourcePool)
    {
        if (const auto& existing = resourcePool->getResourceByUrl(path))
            return existing;
    }

    if (!QFileInfo::exists(path))
        return QnResourcePtr();

    return createArchiveResource(path, resourcePool);
}

QnResourcePtr ResourceDirectoryBrowser::createArchiveResource(
    const QString& filename,
    const QPointer<QnResourcePool>& resourcePool)
{
    const QString path = QDir::fromNativeSeparators(filename);
    NX_ASSERT(path == filename);

    if (FileTypeSupport::isMovieFileExt(path))
        return QnResourcePtr(new QnAviResource(path, qnClientCoreModule->commonModule()));

    if (FileTypeSupport::isImageFileExt(path))
    {
        QnResourcePtr res =
            QnResourcePtr(new QnAviResource(path, qnClientCoreModule->commonModule()));
        res->addFlags(Qn::still_image);
        res->removeFlags(Qn::video | Qn::audio);
        return res;
    }

    if (resourcePool)
    {
        if (FileTypeSupport::isValidLayoutFile(path))
            return layoutFromFile(path, resourcePool);
    }

    return QnResourcePtr();
}

void ResourceDirectoryBrowser::dropResourcesFromDirectory(const QString& path)
{
    QnResourcePool* resourcePool = qnClientCoreModule->commonModule()->resourcePool();
    const QString canonicalDirPath = QDir(path).canonicalPath();

    QnResourceList resourcesToRemove;
    const auto shouldBeRemoved =
        [canonicalDirPath](const QnResourcePtr& resource) -> bool
        {
            if (!resource->hasFlags(Qn::local))
                return false;
            const auto canonicalFilePath = QFileInfo(resource->getUniqueId()).canonicalFilePath();
            return canonicalFilePath.startsWith(canonicalDirPath);
        };

    for (const auto& aviResource: resourcePool->getResources<QnAviResource>())
    {
        if(shouldBeRemoved(aviResource))
            resourcesToRemove.append(aviResource);
    }
    for (const auto& fileLayoutResource: resourcePool->getResources<QnFileLayoutResource>())
    {
        if (shouldBeRemoved(fileLayoutResource))
            resourcesToRemove.append(fileLayoutResource);
    }
    NX_DEBUG(this, "Drop resources from directory");
    resourcePool->removeResources(resourcesToRemove);
}

void ResourceDirectoryBrowser::stopInternal()
{
    m_localResourceDirectoryModel->disconnect(m_resourceProducer);

    m_resourceProducerThread->requestInterruption();
    m_resourceProducerThread->exit();
    m_resourceProducerThread->wait();
}

LocalResourceProducer::LocalResourceProducer(UpdateFileLayoutHelper* helper, QObject* parent):
    QObject(parent),
    m_updateFileLayoutHelper(helper)
{
}

void LocalResourceProducer::createLocalResources(const QStringList& pathList)
{
    if (QThread::currentThread()->isInterruptionRequested())
        return;

    auto clientCoreModule = qnClientCoreModule;
    if (!clientCoreModule)
        return;

    QPointer<QnResourcePool> resourcePool(clientCoreModule->commonModule()->resourcePool());

    QnResourceList newResources;
    for (const auto& filePath: pathList)
    {
        if (QThread::currentThread()->isInterruptionRequested())
            return;

        QnResourcePtr resource =
            ResourceDirectoryBrowser::createArchiveResource(filePath, resourcePool);
        if (!resource)
            continue;
        if (resource->getId().isNull() && !resource->getUniqueId().isEmpty())
        {
            // Create same IDs for the same files.
            resource->setIdUnsafe(QnUuid::fromArbitraryData(resource->getUniqueId().toUtf8()));
        }

        newResources.append(resource);
    }
    if (QThread::currentThread()->isInterruptionRequested())
        return;

    resourcePool->addResources(newResources);
}

void LocalResourceProducer::updateFileLayoutResource(const QString& path)
{
    if (QThread::currentThread()->isInterruptionRequested())
        return;

    QnResourcePool* resourcePool = qnClientCoreModule->commonModule()->resourcePool();
    if (auto fileLayout =
        resourcePool->getResourceByUrl(path).dynamicCast<QnFileLayoutResource>())
    {
        // Remove layouts items in the main thread.
        QMetaObject::invokeMethod(m_updateFileLayoutHelper,
            "startUpdateLayout",
            Qt::BlockingQueuedConnection,
            Q_ARG(QnFileLayoutResourcePtr, fileLayout));

        QString password = layout::password(fileLayout);
        QnFileLayoutResourcePtr updatedLayout =
            layout::layoutFromFile(fileLayout->getUrl(), password);
        if (!updatedLayout)
            return;

        if (QThread::currentThread()->isInterruptionRequested())
            return;

        // Update source layout using loaded layout in the main thread.
        QMetaObject::invokeMethod(m_updateFileLayoutHelper,
            "finishUpdateLayout",
            Qt::BlockingQueuedConnection,
            Q_ARG(QnFileLayoutResourcePtr, updatedLayout));
    }
}

void LocalResourceProducer::updateVideoFileResource(const QString& path)
{
    if (QThread::currentThread()->isInterruptionRequested())
        return;

    auto clientCoreModule = qnClientCoreModule;
    if (!clientCoreModule)
        return;

    const auto resourcePool = clientCoreModule->commonModule()->resourcePool();

    if (auto aviResource = resourcePool->getResourceByUrl(path).dynamicCast<QnAviResource>())
    {
        if (QThread::currentThread()->isInterruptionRequested())
            return;

        resourcePool->removeResource(aviResource);

        if (QThread::currentThread()->isInterruptionRequested())
            return;

        auto updatedAviResource =
            QnResourcePtr(new QnAviResource(path, clientCoreModule->commonModule()));
        resourcePool->addResource(updatedAviResource);
    }
}

} // namespace nx::vms::client::desktop
