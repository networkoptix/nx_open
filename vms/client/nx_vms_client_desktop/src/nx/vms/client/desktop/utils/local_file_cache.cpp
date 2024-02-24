// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "local_file_cache.h"

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QStandardPaths>

#include <nx/utils/log/assert.h>

namespace nx::vms::client::desktop {

namespace {

/** Check if file is stored in the application data. Such files must not be copied elsewhere. */
bool isResourceFile(const QString& filename)
{
    return filename.startsWith(":/") || filename.startsWith("qrc://");
}

} // namespace

LocalFileCache::LocalFileCache(SystemContext* systemContext, QObject* parent):
    base_type(systemContext, parent)
{
}

LocalFileCache::~LocalFileCache()
{
}

QString LocalFileCache::fullPath(const QString& filename, const QString& folder)
{
    if (isResourceFile(filename))
        return filename;

    QString cachedName = filename;
    if (QDir::isAbsolutePath(filename))
        cachedName = ServerImageCache::cachedImageFilename(filename);

    QString path = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    return QDir::toNativeSeparators(QString("%1/cache/local/%2/%3")
        .arg(path)
        .arg(folder)
        .arg(cachedName)
    );
}

QString LocalFileCache::getFullPath(const QString& filename) const
{
    return fullPath(filename, folderName());
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

    if (isResourceFile(filename))
    {
        NX_ASSERT(QFileInfo(filename).exists());
        emit fileDownloaded(filename, OperationResult::ok);
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

} // namespace nx::vms::client::desktop
