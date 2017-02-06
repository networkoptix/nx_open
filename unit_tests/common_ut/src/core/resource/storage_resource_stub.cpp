#include "storage_resource_stub.h"

QnStorageResourceStub::QnStorageResourceStub():
    base_type()
{
    setId(QnUuid::createUuid());
}

QnStorageResourceStub::~QnStorageResourceStub()
{
}

QIODevice* QnStorageResourceStub::open(const QString&, QIODevice::OpenMode)
{
    NX_ASSERT(false);
    return NULL;
}

Qn::StorageInitResult QnStorageResourceStub::initOrUpdate()
{
    NX_ASSERT(false);
    return Qn::StorageInit_CreateFailed;
}

QnAbstractStorageResource::FileInfoList QnStorageResourceStub::getFileList(const QString&)
{
    NX_ASSERT(false);
    return QnAbstractStorageResource::FileInfoList();
}

qint64 QnStorageResourceStub::getFileSize(const QString&) const
{
    NX_ASSERT(false);
    return 0;
}

bool QnStorageResourceStub::removeFile(const QString&)
{
    NX_ASSERT(false);
    return false;
}

bool QnStorageResourceStub::removeDir(const QString&)
{
    NX_ASSERT(false);
    return false;
}

bool QnStorageResourceStub::renameFile(const QString&, const QString&)
{
    NX_ASSERT(false);
    return false;
}

bool QnStorageResourceStub::isFileExists(const QString&)
{
    NX_ASSERT(false);
    return false;
}

bool QnStorageResourceStub::isDirExists(const QString&)
{
    NX_ASSERT(false);
    return false;
}

qint64 QnStorageResourceStub::getFreeSpace()
{
    NX_ASSERT(false);
    return 0;
}

qint64 QnStorageResourceStub::getTotalSpace() const
{
    NX_ASSERT(false);
    return 0;
}

int QnStorageResourceStub::getCapabilities() const
{
    NX_ASSERT(false);
    return 0;
}


