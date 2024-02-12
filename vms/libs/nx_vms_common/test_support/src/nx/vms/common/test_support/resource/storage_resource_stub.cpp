// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "storage_resource_stub.h"

namespace nx {

StorageResourceStub::StorageResourceStub():
    base_type()
{
    setIdUnsafe(nx::Uuid::createUuid());
}

StorageResourceStub::~StorageResourceStub()
{
}

QIODevice* StorageResourceStub::openInternal(const QString&, QIODevice::OpenMode)
{
    NX_ASSERT(false);
    return NULL;
}

nx::vms::api::StorageInitResult StorageResourceStub::initOrUpdate()
{
    NX_ASSERT(false);
    return nx::vms::api::StorageInitResult::createFailed;
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
