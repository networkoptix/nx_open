#include "file_processor.h"

#include <QtCore/QDirIterator>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>

#include <common/common_module.h>
#include <client_core/client_core_module.h>

#include <QtWidgets/QApplication>

#include <core/resource/resource_directory_browser.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/avi/filetypesupport.h>
#include <core/storage/file_storage/layout_storage_resource.h>

#include <ui/workaround/mac_utils.h>

namespace {

QString fixSeparators(const QString& filePath)
{
    return QDir::fromNativeSeparators(filePath);
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

QnResourcePtr QnFileProcessor::createResourcesForFile(const QString& fileName)
{
    const auto path = fixSeparators(fileName);
    auto pool = qnClientCoreModule->commonModule()->resourcePool();
    const auto result = QnResourceDirectoryBrowser::resourceFromFile(path, pool);
    if (result)
        pool->addResource(result);
    return result;
}

QnResourceList QnFileProcessor::createResourcesForFiles(const QStringList &files)
{
    auto pool = qnClientCoreModule->commonModule()->resourcePool();
    QnResourceList result;
    for (const auto& fileName: files)
    {
        const auto path = fixSeparators(fileName);
        QnResourcePtr resource = QnResourceDirectoryBrowser::resourceFromFile(path, pool);
        if (resource)
            result << resource;
    }
    pool->addResources(result);

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

        auto resource = QnResourceDirectoryBrowser::resourceFromFile(filePath, resourcePool);
        if (resource)
            result << resource;

    }

    return result;
}

void QnFileProcessor::deleteLocalResources(const QnResourceList &resources_)
{
    QnResourceList resources;
    foreach (const QnResourcePtr &resource, resources_)
        if (resource->hasFlags(Qn::url | Qn::local))
            resources.append(resource);
    if (resources.isEmpty())
        return;

    qnClientCoreModule->commonModule()->resourcePool()->removeResources(resources);
    for (const QnResourcePtr& resource: resources)
        QFile::remove(resource->getUrl());
}
