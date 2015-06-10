#include <cassert>
#include "client_storage_resource.h"

QIODevice* QnClientStorageResource::open(const QString&, QIODevice::OpenMode)
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

QFileInfoList QnClientStorageResource::getFileList(const QString&)
{
    assert(false);
    return QFileInfoList();
}

qint64 QnClientStorageResource::getFileSize(const QString&) const
{
    assert(false);
    return 0;
}

bool QnClientStorageResource::removeFile(const QString&)
{
    assert(false);
    return false;
}

bool QnClientStorageResource::removeDir(const QString&)
{
    assert(false);
    return false;
}

bool QnClientStorageResource::renameFile(const QString&, const QString&)
{
    assert(false);
    return false;
}

bool QnClientStorageResource::isFileExists(const QString&)
{
    assert(false);
    return false;
}

bool QnClientStorageResource::isDirExists(const QString&)
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
