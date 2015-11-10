#include "storage_status_reply.h"

#include <core/resource/storage_resource.h>

QnStorageSpaceData::QnStorageSpaceData() 
    : url()
    , storageId()
    , totalSpace(QnStorageResource::UnknownSize)
    , freeSpace(QnStorageResource::UnknownSize)
    , reservedSpace(0)
    , isExternal(false)
    , isWritable(false)
    , isUsedForWriting(false)
    , isBackup(false)
    , storageType()
{}

QnStorageSpaceData::QnStorageSpaceData( const QnStorageResourcePtr &storage )
    : url(storage->getUrl())
    , storageId(storage->getId())
    , totalSpace(storage->getTotalSpace())
    , freeSpace(storage->getFreeSpace())
    , reservedSpace(storage->getSpaceLimit())
    , isExternal(storage->isExternal())
    , isWritable(storage->isWritable())
    , isUsedForWriting(storage->isUsedForWriting())
    , isBackup(storage->isBackup())
    , storageType(storage->getStorageType())
{}

QnStorageStatusReply::QnStorageStatusReply()
    : pluginExists(true)
    , storage()
{}
