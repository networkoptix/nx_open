// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>
#include <api/model/api_model_fwd.h>
#include <nx/utils/uuid.h>

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

    QnStorageModelInfo();
    explicit QnStorageModelInfo(const nx::vms::api::StorageSpaceData& reply);
    explicit QnStorageModelInfo(const QnStorageResourcePtr& storage);
};

typedef QList<QnStorageModelInfo> QnStorageModelInfoList;

Q_DECLARE_METATYPE(QnStorageModelInfo)
