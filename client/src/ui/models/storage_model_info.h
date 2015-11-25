#pragma once

#include <core/resource/resource_fwd.h>

#include <nx/tool/uuid.h>

struct QnStorageSpaceData;

struct QnStorageModelInfo {
    QnUuid id;
    bool isUsed;
    QString url;
    QString storageType;
    qint64 totalSpace;
    bool isWritable;
    bool isBackup;
    bool isExternal;

    QnStorageModelInfo();
    explicit QnStorageModelInfo(const QnStorageSpaceData &reply);
    explicit QnStorageModelInfo(const QnStorageResourcePtr &storage);
};

typedef QList<QnStorageModelInfo> QnStorageModelInfoList;

Q_DECLARE_METATYPE(QnStorageModelInfo)
