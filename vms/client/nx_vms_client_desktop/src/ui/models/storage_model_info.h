// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <api/model/api_model_fwd.h>
#include <core/resource/resource_fwd.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/data/storage_archive_mode.h>
#include <nx/vms/api/data/storage_space_data.h>

struct QnStorageModelInfo
{
    QnUuid id;
    bool isUsed;
    QString url;
    QString storageType;
    qint64 totalSpace;
    qint64 reservedSpace;
    bool isWritable;
    bool isBackup;
    bool isExternal;
    bool isOnline;
    bool isDbReady;
    nx::vms::api::StorageArchiveMode archiveMode = nx::vms::api::StorageArchiveMode::undefined;

    QnStorageModelInfo();
    explicit QnStorageModelInfo(const nx::vms::api::StorageSpaceDataV1& reply);
    explicit QnStorageModelInfo(const QnStorageResourcePtr& storage);
};

typedef QList<QnStorageModelInfo> QnStorageModelInfoList;
