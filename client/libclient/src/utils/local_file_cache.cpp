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

QString QnLocalFileCache::getFullPath(const QString &filename) const
{
    QString cachedName = filename;
    if (QDir::isAbsolutePath(filename))
        cachedName = QnAppServerImageCache::cachedImageFilename(filename);

    QString path = QStandardPaths::writableLocation(QStandardPaths::DataLocation);
    return QDir::toNativeSeparators(QString(lit("%1/cache/local/%2/%3"))
                                    .arg(path)
                                    .arg(folderName())
                                    .arg(cachedName)
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

    image.save(fullPath);
}

void QnLocalFileCache::downloadFile(const QString &filename)
{
    if (filename.isEmpty())
    {
        emit fileDownloaded(filename, OperationResult::invalidOperation);
        return;
    }

    QString cachedName = filename;

    /* Full image path. */
    if (QDir::isAbsolutePath(filename))
    {
        QImage source(filename);
        if (source.isNull())
        {
            emit fileDownloaded(filename, OperationResult::fileSystemError);
            return;
        }

        cachedName = QnAppServerImageCache::cachedImageFilename(filename);
        storeImageData(cachedName, source);
    }

    QFileInfo info(getFullPath(cachedName));
    emit fileDownloaded(filename, info.exists() ? OperationResult::ok : OperationResult::fileSystemError);
}

void QnLocalFileCache::uploadFile(const QString &filename) {
    emit fileUploaded(filename, OperationResult::ok);
}
