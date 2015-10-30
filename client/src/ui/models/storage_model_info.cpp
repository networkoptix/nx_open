#include "storage_model_info.h"

#include <api/model/storage_status_reply.h>

#include <core/resource/client_storage_resource.h>

QnStorageModelInfo::QnStorageModelInfo()
    : id()
    , isUsed(true)
    , url()
    , storageType()
    , totalSpace(QnStorageResource::UnknownSize)
    , isWritable(false)
    , isBackup(false)
    , isExternal(false)
{}

QnStorageModelInfo::QnStorageModelInfo( const QnStorageSpaceData &reply )
    : id(reply.storageId)
    , isUsed(reply.isUsedForWriting)
    , url(reply.url)
    , storageType(reply.storageType)
    , totalSpace(reply.totalSpace)
    , isWritable(reply.isWritable)
    , isBackup(reply.isBackup)
    , isExternal(reply.isExternal)
{}

