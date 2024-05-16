// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "storage_model_info.h"

#include <api/model/storage_status_reply.h>
#include <nx/vms/api/data/storage_flags.h>
#include <nx/vms/api/data/storage_space_data.h>
#include <nx/vms/client/core/resource/client_storage_resource.h>

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
    , isDbReady(false)
{}

QnStorageModelInfo::QnStorageModelInfo(const nx::vms::api::StorageSpaceDataV1& reply)
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
    , isDbReady(reply.storageStatus.testFlag(nx::vms::api::StorageStatus::dbReady))
{
    archiveMode = isExternal
        ? nx::vms::api::StorageArchiveMode::isolated
        : nx::vms::api::StorageArchiveMode::exclusive;
}

QnStorageModelInfo::QnStorageModelInfo( const QnStorageResourcePtr& storage)
    : id(storage->getId())
    , isUsed(storage->isUsedForWriting())
    , url(storage->getUrl())
    , storageType(storage->getStorageType())
    , totalSpace(storage->getTotalSpace())
    , isWritable(storage->isWritable())
    , isBackup(storage->isBackup())
    , isExternal(storage->isExternal())
    , isOnline(storage->getStatus() == nx::vms::api::ResourceStatus::online)
    , isDbReady(storage->persistentStatusFlags().testFlag(
        nx::vms::api::StoragePersistentFlag::dbReady))
    , archiveMode(storage->storageArchiveMode())
{}
