#include "local_file_cache.h"

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtGui/QDesktopServices>

QnLocalFileCache::QnLocalFileCache(QObject *parent) :
    base_type(parent)
{
}

QnLocalFileCache::~QnLocalFileCache() {

}

QString QnLocalFileCache::getFullPath(const QString &filename) const {
    QString path = QDesktopServices::storageLocation(QDesktopServices::DataLocation);
    return QDir::toNativeSeparators(QString(QLatin1String("%1/cache/local/%2/%3"))
                                    .arg(path)
                                    .arg(folderName())
                                    .arg(filename)
                                    );
}

void QnLocalFileCache::storeImage(const QString &fileName, const QByteArray &image) {
    ensureCacheFolder();
    QString fullPath = getFullPath(fileName);
    if (QFileInfo(fullPath).exists())
        return;

    QFile file(fullPath);
    file.open(QIODevice::WriteOnly);
    file.write(image);
    file.close();
}

void QnLocalFileCache::downloadFile(const QString &filename) {
    if (filename.isEmpty()) {
        emit fileDownloaded(filename, false);
        return;
    }

    QFileInfo info(getFullPath(filename));
    emit fileDownloaded(filename, info.exists());
}
