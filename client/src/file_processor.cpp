#include "file_processor.h"

#include <QtCore/QDirIterator>
#include <QtCore/QFileInfo>

#include <core/resource/directory_browser.h>
#include <core/resourcemanagment/resource_pool.h>
#include <plugins/resources/archive/avi_files/avi_dvd_device.h>
#include <plugins/resources/archive/avi_files/avi_bluray_device.h>
#include <plugins/resources/archive/avi_files/avi_dvd_archive_delegate.h>
#include <plugins/resources/archive/filetypesupport.h>

QStringList QnFileProcessor::findAcceptedFiles(const QStringList &files)
{
    QStringList acceptedFiles;
    foreach (const QString &path, files) {
        if (CLAviDvdDevice::isAcceptedUrl(path)) {
            if (path.indexOf(QLatin1Char('?')) == -1) {
                /* Open all titles on a DVD. */
                QStringList titles = QnAVIDvdArchiveDelegate::getTitleList(path);
                foreach (const QString &title, titles)
                    acceptedFiles.append(path + QLatin1String("?title=") + title);
            } else {
                acceptedFiles.append(path);
            }
        } else if (CLAviBluRayDevice::isAcceptedUrl(path)) {
            acceptedFiles.append(path);
        } else {
            FileTypeSupport fileTypeSupport;
            QFileInfo fileInfo(path);
            if (fileInfo.isDir()) {
                QDirIterator it(path, QDirIterator::Subdirectories);
                while (it.hasNext()) {
                    QString nextFilename = it.next();
                    if (it.fileInfo().isFile() && fileTypeSupport.isFileSupported(nextFilename))
                        acceptedFiles.append(nextFilename);
                }
            } else if (fileInfo.isFile() && fileTypeSupport.isFileSupported(path)) {
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

QnResourcePtr QnFileProcessor::createResourcesForFile(const QString &file)
{
    QnResourcePtr result = QnResourceDirectoryBrowser::instance().checkFile(file);

    qnResPool->addResource(result);

    return result;
}

QnResourceList QnFileProcessor::createResourcesForFiles(const QList<QString> &files)
{
    QnResourceList result = QnResourceDirectoryBrowser::instance().checkFiles(files);

    qnResPool->addResources(result);

    return result;
}

