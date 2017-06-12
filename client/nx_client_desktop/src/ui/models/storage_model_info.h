#pragma once

#include <core/resource/resource_fwd.h>

#include <nx/utils/uuid.h>

struct QnStorageSpaceData;

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

    QnStorageModelInfo();
    explicit QnStorageModelInfo(const QnStorageSpaceData &reply);
    explicit QnStorageModelInfo(const QnStorageResourcePtr &storage);
};

typedef QList<QnStorageModelInfo> QnStorageModelInfoList;

Q_DECLARE_METATYPE(QnStorageModelInfo)
