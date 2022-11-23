// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "file_processor.h"

#include <QtCore/QDirIterator>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtWidgets/QApplication>

#include <core/resource/avi/filetypesupport.h>
#include <core/resource/resource_directory_browser.h>
#include <core/resource_management/resource_pool.h>
#include <core/storage/file_storage/layout_storage_resource.h>
#include <ui/workaround/mac_utils.h>

using namespace nx::vms::client::desktop;

namespace {

QString fixSeparators(const QString& filePath)
{
    return QDir::fromNativeSeparators(filePath);
}

void fixSeparators(QStringList& filePaths)
{
    std::transform(filePaths.cbegin(), filePaths.cend(), filePaths.begin(),
        [](const QString& path) { return fixSeparators(path); });
}

QnResourcePtr resourceFromFile(
    const QString& filename,
    const QPointer<QnResourcePool>& resourcePool)
{
    const QString path = fixSeparators(filename);
    if (resourcePool)
    {
        if (const auto& existing = resourcePool->getResourceByUrl(path))
            return existing;
    }

    if (!QFileInfo::exists(path))
        return QnResourcePtr();

    return ResourceDirectoryBrowser::createArchiveResource(path, resourcePool);
}

} // namespace

QStringList QnFileProcessor::findAcceptedFiles(const QStringList &files)
{
    QStringList acceptedFiles;
    for (QString path: files)
    {
        path = fixSeparators(path);
        QFileInfo fileInfo(path);
        if (fileInfo.isDir())
        {
            QDirIterator it(path, QDirIterator::Subdirectories);
            while (it.hasNext())
            {
                QString nextFilename = it.next();
                if (it.fileInfo().isFile() && FileTypeSupport::isFileSupported(nextFilename))
                    acceptedFiles.append(nextFilename);
            }
        }
        else if (fileInfo.isFile() && FileTypeSupport::isFileSupported(path))
        {
            acceptedFiles.append(path);
        }
    }

    return acceptedFiles;
}

QStringList QnFileProcessor::findAcceptedFiles(const QList<QUrl>& urls)
{
    QStringList files;
    for (const QUrl& url: urls)
    {
        if (!url.isLocalFile())
            continue;

        files.append(fixSeparators(url.toLocalFile()));
    }
    return QnFileProcessor::findAcceptedFiles(files);
}

QnResourcePtr QnFileProcessor::findResourceForFile(
    const QString& fileName,
    QnResourcePool* resourcePool)
{
    if (resourcePool)
    {
        if (const auto& existing = resourcePool->getResourceByUrl(fixSeparators(fileName)))
            return existing;
    }
    return {};
}

QnResourcePtr QnFileProcessor::createResourcesForFile(
    const QString& fileName,
    QnResourcePool* resourcePool)
{
    if (!NX_ASSERT(resourcePool))
        return {};

    const auto result = createResourcesForFiles({fileName}, resourcePool);
    return result.isEmpty() ? QnResourcePtr() : result.first();
}

QnResourceList QnFileProcessor::createResourcesForFiles(
    QStringList files,
    QnResourcePool* resourcePool)
{
    if (!NX_ASSERT(resourcePool))
        return {};

    fixSeparators(files);

    QnResourceList result;

    for (const auto& path: files)
    {
        if (!QFileInfo::exists(path))
            continue;

        if (auto existingResource = resourcePool->getResourceByUrl(path))
            resourcePool->removeResource(existingResource);

        if (auto newResource = ResourceDirectoryBrowser::createArchiveResource(path, resourcePool))
            result << newResource;
    }

    resourcePool->addResources(result);

    return result;
}

QnResourceList QnFileProcessor::findOrCreateResourcesForFiles(const QList<QUrl>& urls,
    QnResourcePool* resourcePool)
{
    QnResourceList result;

    for (const auto& url: urls)
    {
        if (!url.isLocalFile())
            continue;

        const auto filePath = fixSeparators(url.toLocalFile());
        if (!QFile::exists(filePath))
            continue;

        if (!FileTypeSupport::isFileSupported(filePath))
            continue;

        #if defined(Q_OS_MAC)
            mac_saveFileBookmark(url.path());
        #endif

        auto resource = resourceFromFile(filePath, resourcePool);
        if (resource)
            result << resource;

    }

    return result;
}

void QnFileProcessor::deleteLocalResources(const QnResourceList& resources)
{
    for (const QnResourcePtr& resource: resources)
    {
        if (resource->hasFlags(Qn::url | Qn::local))
        {
            if (auto resourcePool = resource->resourcePool())
                resourcePool->removeResource(resource);
            QFile::remove(resource->getUrl());
        }
    }
}
