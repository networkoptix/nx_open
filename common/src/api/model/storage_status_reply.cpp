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
    , storageType()
{}

QnStorageStatusReply::QnStorageStatusReply()
    : pluginExists(true)
    , storage()
{}
