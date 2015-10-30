#pragma once

#include <utils/common/uuid.h>

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
    QnStorageModelInfo(const QnStorageSpaceData &reply);
};

typedef QList<QnStorageModelInfo> QnStorageModelInfoList;

Q_DECLARE_METATYPE(QnStorageModelInfo)
