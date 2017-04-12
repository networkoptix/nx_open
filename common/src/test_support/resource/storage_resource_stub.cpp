#include "storage_resource_stub.h"

namespace nx {

StorageResourceStub::StorageResourceStub():
    base_type(nullptr)
{
    setId(QnUuid::createUuid());
}

StorageResourceStub::~StorageResourceStub()
{
}

QIODevice* StorageResourceStub::open(const QString&, QIODevice::OpenMode)
{
    NX_ASSERT(false);
    return NULL;
}

Qn::StorageInitResult StorageResourceStub::initOrUpdate()
{
    NX_ASSERT(false);
    return Qn::StorageInit_CreateFailed;
}

QnAbstractStorageResource::FileInfoList StorageResourceStub::getFileList(const QString&)
{
    NX_ASSERT(false);
    return QnAbstractStorageResource::FileInfoList();
}

qint64 StorageResourceStub::getFileSize(const QString&) const
{
    NX_ASSERT(false);
    return 0;
}

bool StorageResourceStub::removeFile(const QString&)
{
    NX_ASSERT(false);
    return false;
}

bool StorageResourceStub::removeDir(const QString&)
{
    NX_ASSERT(false);
    return false;
}

bool StorageResourceStub::renameFile(const QString&, const QString&)
{
    NX_ASSERT(false);
    return false;
}

bool StorageResourceStub::isFileExists(const QString&)
{
    NX_ASSERT(false);
    return false;
}

bool StorageResourceStub::isDirExists(const QString&)
{
    NX_ASSERT(false);
    return false;
}

qint64 StorageResourceStub::getFreeSpace()
{
    NX_ASSERT(false);
    return 0;
}

qint64 StorageResourceStub::getTotalSpace() const
{
    NX_ASSERT(false);
    return 0;
}

int StorageResourceStub::getCapabilities() const
{
    NX_ASSERT(false);
    return 0;
}

} // namespace nx
