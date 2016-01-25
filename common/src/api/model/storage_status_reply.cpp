#include "storage_status_reply.h"

#include <core/resource/storage_resource.h>

QnStorageSpaceData::QnStorageSpaceData()
    : url()
    , storageId()
    , totalSpace(QnStorageResource::kUnknownSize)
    , freeSpace(QnStorageResource::kUnknownSize)
    , reservedSpace(0)
    , isExternal(false)
    , isWritable(false)
    , isUsedForWriting(false)
    , isBackup(false)
    , isOnline(false)
    , storageType()
{}

QnStorageSpaceData::QnStorageSpaceData( const QnStorageResourcePtr &storage, bool fastCreate)
    : url               (storage->getUrl())
    , storageId         (storage->getId())
    , totalSpace        (fastCreate ? QnStorageResource::kFastCreateSize : storage->getTotalSpace())
    , freeSpace         (fastCreate ? QnStorageResource::kFastCreateSize : storage->getFreeSpace())
    , reservedSpace     (storage->getSpaceLimit())
    , isExternal        (storage->isExternal())
    , isWritable        (storage->isWritable()) //TODO: #GDM check if it is really fast
    , isUsedForWriting  (storage->isUsedForWriting())
    , isBackup          (storage->isBackup())
    , isOnline          (storage->getStatus() == Qn::Online)
    , storageType       (storage->getStorageType())
{}

QnStorageStatusReply::QnStorageStatusReply()
    : pluginExists(true)
    , storage()
{}
