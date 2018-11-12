#pragma once

#include <api/model/api_model_fwd.h>

#include <QtCore/QMetaType>

#include <core/resource/resource_fwd.h>

#include <nx/fusion/model_functions_fwd.h>
#include <utils/common/id.h>
#include <common/common_globals.h>

struct QnStorageSpaceData {
    QString url;
    QnUuid storageId;
    qint64 totalSpace;
    qint64 freeSpace;
    qint64 reservedSpace;
    bool isExternal;
    bool isWritable;
    bool isUsedForWriting;
    bool isBackup;
    bool isOnline;
    QString storageType;
    Qn::StorageStatuses storageStatus;

    QnStorageSpaceData();
    explicit QnStorageSpaceData(const QnStorageResourcePtr &storage, bool fastCreate);
};
#define QnStorageSpaceData_Fields (url)(storageId)(totalSpace)(freeSpace)(reservedSpace) \
    (isExternal)(isWritable)(isUsedForWriting)(storageType)(isBackup)(isOnline)(storageStatus)

struct QnStorageStatusReply {
    bool pluginExists;
    QnStorageSpaceData storage;
    Qn::StorageInitResult status;

    QnStorageStatusReply();
};

#define QnStorageStatusReply_Fields (pluginExists)(storage)(status)

QN_FUSION_DECLARE_FUNCTIONS(QnStorageSpaceData, (eq)(metatype))
QN_FUSION_DECLARE_FUNCTIONS(QnStorageStatusReply, (json)(metatype))
