#include "file_processor.h"

#include <QtCore/QDirIterator>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>

#include <QtWidgets/QApplication>

#include <core/resource/resource_directory_browser.h>
#include <core/resource_management/resource_pool.h>
#include <plugins/resource/avi/avi_dvd_resource.h>
#include <plugins/resource/avi/avi_bluray_resource.h>
#include <plugins/resource/avi/avi_dvd_archive_delegate.h>
#include <plugins/resource/avi/filetypesupport.h>
#include <plugins/storage/file_storage/layout_storage_resource.h>

QStringList QnFileProcessor::findAcceptedFiles(const QStringList &files)
{
    QStringList acceptedFiles;
    foreach (const QString &path, files) {
        if (QnAviDvdResource::isAcceptedUrl(path)) {
            if (path.indexOf(QLatin1Char('?')) == -1) {
                /* Open all titles on a DVD. */
                QStringList titles = QnAVIDvdArchiveDelegate::getTitleList(path);
                foreach (const QString &title, titles)
                    acceptedFiles.append(path + QLatin1String("?title=") + title);
            } else {
                acceptedFiles.append(path);
            }
        } else if (QnAviBlurayResource::isAcceptedUrl(path)) {
            acceptedFiles.append(path);
        } else {
            QFileInfo fileInfo(path);
            if (fileInfo.isDir()) {
                QDirIterator it(path, QDirIterator::Subdirectories);
                while (it.hasNext()) {
                    QString nextFilename = it.next();
                    if (it.fileInfo().isFile() && FileTypeSupport::isFileSupported(nextFilename))
                        acceptedFiles.append(nextFilename);
                }
            } else if (fileInfo.isFile() && FileTypeSupport::isFileSupported(path)) {
                acceptedFiles.append(path);
            }
        }
    }

    return acceptedFiles;
}

QStringList QnFileProcessor::findAcceptedFiles(const QList<QUrl> &urls)
{
    QStringList files;
    foreach (const QUrl &url, urls)
        files.append(url.toLocalFile());
    return QnFileProcessor::findAcceptedFiles(files);
}

QnResourcePtr QnFileProcessor::createResourcesForFile(const QString& fileName)
{
    QnResourcePtr result = QnResourceDirectoryBrowser::resourceFromFile(fileName);
    if (result)
        resourcePool()->addResource(result);
    return result;
}

QnResourceList QnFileProcessor::createResourcesForFiles(const QStringList &files)
{
    QnResourceList result;
    for (const QString& fileName : files)
    {
        QnResourcePtr resource = QnResourceDirectoryBrowser::resourceFromFile(fileName);
        if (resource)
            result << resource;
    }
    resourcePool()->addResources(result);

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

    resourcePool()->removeResources(resources);
    for (const QnResourcePtr& resource: resources)
        QFile::remove(resource->getUrl());
}
