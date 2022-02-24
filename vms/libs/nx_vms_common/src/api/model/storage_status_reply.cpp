// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "storage_status_reply.h"

#include <core/resource/storage_resource.h>

nx::vms::api::StorageSpaceData fromResourceToApi(
    const QnStorageResourcePtr& storage, bool fastCreate)
{
    nx::vms::api::StorageSpaceData r;
    r.url = storage->getUrl();
    r.storageId = storage->getId();
    r.totalSpace =
        fastCreate ? QnStorageResource::kSizeDetectionOmitted : storage->getTotalSpace();
    r.freeSpace = fastCreate ? QnStorageResource::kSizeDetectionOmitted : storage->getFreeSpace();
    r.reservedSpace = storage->getSpaceLimit();
    r.isExternal = storage->isExternal();
    r.isWritable = fastCreate ? true : storage->isWritable();
    r.isUsedForWriting = storage->isUsedForWriting();
    r.isBackup = storage->isBackup();
    r.isOnline = storage->getStatus() == nx::vms::api::ResourceStatus::online;
    r.storageType = storage->getStorageType();
    return r;
}

QnStorageStatusReply::QnStorageStatusReply()
    : pluginExists(false)
    , status(Qn::StorageInit_CreateFailed)
{}
