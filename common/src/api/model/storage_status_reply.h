#pragma once

#include <api/model/api_model_fwd.h>

#include <QtCore/QMetaType>

#include <core/resource/resource_fwd.h>

#include <utils/common/model_functions_fwd.h>
#include <utils/common/id.h>

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
    QString storageType;

    QnStorageSpaceData();
    explicit QnStorageSpaceData(const QnStorageResourcePtr &storage);
};
#define QnStorageSpaceData_Fields (url)(storageId)(totalSpace)(freeSpace)(reservedSpace)(isExternal)(isWritable)(isUsedForWriting)(storageType)(isBackup)

struct QnStorageStatusReply {
    bool pluginExists;
    QnStorageSpaceData storage;

    QnStorageStatusReply();
};

#define QnStorageStatusReply_Fields (pluginExists)(storage)

QN_FUSION_DECLARE_FUNCTIONS(QnStorageSpaceData, (eq)(metatype))
QN_FUSION_DECLARE_FUNCTIONS(QnStorageStatusReply, (json)(metatype))
