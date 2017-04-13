#include "local_file_cache.h"

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QStandardPaths>

namespace nx {
namespace client {
namespace desktop {

LocalFileCache::LocalFileCache(QObject* parent):
    base_type(parent)
{
}

LocalFileCache::~LocalFileCache()
{
}

QString LocalFileCache::getFullPath(const QString& filename) const
{
    QString cachedName = filename;
    if (QDir::isAbsolutePath(filename))
        cachedName = ServerImageCache::cachedImageFilename(filename);

    QString path = QStandardPaths::writableLocation(QStandardPaths::DataLocation);
    return QDir::toNativeSeparators(QString(lit("%1/cache/local/%2/%3"))
        .arg(path)
        .arg(folderName())
        .arg(cachedName)
    );
}

void LocalFileCache::storeImageData(const QString& fileName, const QByteArray& imageData)
{
    ensureCacheFolder();
    QString fullPath = getFullPath(fileName);
    if (QFileInfo(fullPath).exists())
        return;

    QFile file(fullPath);
    file.open(QIODevice::WriteOnly);
    file.write(imageData);
    file.close();
}

void LocalFileCache::storeImageData(const QString& fileName, const QImage& image)
{
    ensureCacheFolder();
    QString fullPath = getFullPath(fileName);
    if (QFileInfo(fullPath).exists())
        return;

    image.save(fullPath);
}

void LocalFileCache::downloadFile(const QString& filename)
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

        cachedName = ServerImageCache::cachedImageFilename(filename);
        storeImageData(cachedName, source);
    }

    QFileInfo info(getFullPath(cachedName));
    emit fileDownloaded(filename, info.exists()
        ? OperationResult::ok
        : OperationResult::fileSystemError);
}

void LocalFileCache::uploadFile(const QString& filename)
{
    emit fileUploaded(filename, OperationResult::ok);
}

} // namespace desktop
} // namespace client
} // namespace nx
