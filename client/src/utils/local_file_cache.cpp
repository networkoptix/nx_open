#include "local_file_cache.h"

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QStandardPaths>

QnLocalFileCache::QnLocalFileCache(QObject *parent) :
    base_type(parent)
{
}

QnLocalFileCache::~QnLocalFileCache() {

}

QString QnLocalFileCache::getFullPath(const QString &filename) const {
    QString path = QStandardPaths::writableLocation(QStandardPaths::DataLocation);
    return QDir::toNativeSeparators(QString(lit("%1/cache/local/%2/%3"))
                                    .arg(path)
                                    .arg(folderName())
                                    .arg(filename)
                                    );
}

void QnLocalFileCache::storeImageData(const QString &fileName, const QByteArray &imageData) {
    ensureCacheFolder();
    QString fullPath = getFullPath(fileName);
    if (QFileInfo(fullPath).exists())
        return;

    QFile file(fullPath);
    file.open(QIODevice::WriteOnly);
    file.write(imageData);
    file.close();
}

void QnLocalFileCache::storeImageData(const QString &fileName, const QImage &image) {
    ensureCacheFolder();
    QString fullPath = getFullPath(fileName);
    if (QFileInfo(fullPath).exists())
        return;

    image.save(fullPath, "png");
}

void QnLocalFileCache::downloadFile(const QString &filename) {
    if (filename.isEmpty()) {
        emit fileDownloaded(filename, false);
        return;
    }

    QFileInfo info(getFullPath(filename));
    emit fileDownloaded(filename, info.exists());
}

void QnLocalFileCache::uploadFile(const QString &filename) {
    emit fileUploaded(filename, true);
}
