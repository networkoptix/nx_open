#include "storage_model_info.h"

#include <api/model/storage_status_reply.h>

#include <core/resource/client_storage_resource.h>

QnStorageModelInfo::QnStorageModelInfo()
    : id()
    , isUsed(true)
    , url()
    , storageType()
    , totalSpace(QnStorageResource::kUnknownSize)
    , reservedSpace(0)
    , isWritable(false)
    , isBackup(false)
    , isExternal(false)
    , isOnline(false)
{}

QnStorageModelInfo::QnStorageModelInfo( const QnStorageSpaceData &reply )
    : id(reply.storageId)
    , isUsed(reply.isUsedForWriting)
    , url(reply.url)
    , storageType(reply.storageType)
    , totalSpace(reply.totalSpace)
    , reservedSpace(reply.reservedSpace)
    , isWritable(reply.isWritable)
    , isBackup(reply.isBackup)
    , isExternal(reply.isExternal)
    , isOnline(reply.isOnline)
{}

QnStorageModelInfo::QnStorageModelInfo( const QnStorageResourcePtr &storage )
    : id(storage->getId())
    , isUsed(storage->isUsedForWriting())
    , url(storage->getUrl())
    , storageType(storage->getStorageType())
    , totalSpace(storage->getTotalSpace())
    , isWritable(storage->isWritable())
    , isBackup(storage->isBackup())
    , isExternal(storage->isExternal())
    , isOnline(storage->getStatus() == Qn::Online)
{}

