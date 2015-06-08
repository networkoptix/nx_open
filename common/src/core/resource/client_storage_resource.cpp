#include <cassert>
#include "client_storage_resource.h"

QIODevice* QnClientStorageResource::open(const QString& fileName, QIODevice::OpenMode openMode)
{
    assert(false);
    return NULL;
}

int QnClientStorageResource::getCapabilities() const
{
    assert(false);
    return 0;
}

bool QnClientStorageResource::isAvailable() const
{
    assert(false);
    return 0;
}

QFileInfoList QnClientStorageResource::getFileList(const QString& dirName)
{
    assert(false);
    return QFileInfoList();
}

qint64 QnClientStorageResource::getFileSize(const QString& url) const
{
    assert(false);
    return 0;
}

bool QnClientStorageResource::removeFile(const QString& url)
{
    assert(false);
    return false;
}

bool QnClientStorageResource::removeDir(const QString& url)
{
    assert(false);
    return false;
}

bool QnClientStorageResource::renameFile(const QString& oldName, const QString& newName)
{
    assert(false);
    return false;
}

bool QnClientStorageResource::isFileExists(const QString& url)
{
    assert(false);
    return false;
}

bool QnClientStorageResource::isDirExists(const QString& url)
{
    assert(false);
    return false;
}

qint64 QnClientStorageResource::getFreeSpace()
{
    assert(false);
    return 0;
}

qint64 QnClientStorageResource::getTotalSpace()
{
    assert(false);
    return 0;
}

void QnClientStorageResource::setUrl(const QString& value)
{
    assert(false);
}
