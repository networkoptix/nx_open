#include "file_processor.h"
#include <QDirIterator>
#include <QFileInfo>
#include <plugins/resources/archive/avi_files/avi_dvd_device.h>
#include <plugins/resources/archive/avi_files/avi_bluray_device.h>
#include <plugins/resources/archive/avi_files/avi_dvd_archive_delegate.h>
#include <plugins/resources/archive/filetypesupport.h>
#include <utils/common/warnings.h>

void QnFileProcessor::findAcceptedFiles(const QString &path, QStringList *list) {
    if(list == NULL) {
        qnNullWarning(list);
        return;
    }

    if (CLAviDvdDevice::isAcceptedUrl(path)) {
        if (path.indexOf(QLatin1Char('?')) == -1) {
            /* Open all titles on a DVD. */
            QStringList titles = QnAVIDvdArchiveDelegate::getTitleList(path);
            foreach (const QString &title, titles)
                list->push_back(path + QLatin1String("?title=") + title);
        } else {
            list->push_back(path);
        }
    } else if (CLAviBluRayDevice::isAcceptedUrl(path)) {
        list->push_back(path);
    } else {
        FileTypeSupport fileTypeSupport;
        QFileInfo fileInfo(path);
        if (fileInfo.isDir()) {
            QDirIterator i(path, QDirIterator::Subdirectories);
            while (i.hasNext()) {
                QString nextFilename = i.next();
                if (QFileInfo(nextFilename).isFile()) {
                    if (fileTypeSupport.isFileSupported(nextFilename))
                        list->push_back(nextFilename);
                }
            }
        } else if (fileInfo.isFile()) {
            if (fileTypeSupport.isFileSupported(path))
                list->push_back(path);
        }
    }
}
