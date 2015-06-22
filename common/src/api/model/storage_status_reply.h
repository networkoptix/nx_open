#ifndef QN_STORAGE_STATUS_REPLY_H
#define QN_STORAGE_STATUS_REPLY_H

#include <QtCore/QMetaType>

#include <utils/common/model_functions_fwd.h>
#include <utils/common/id.h>

struct QnStorageSpaceData {
    QnStorageSpaceData() : totalSpace(-1), freeSpace(-1), reservedSpace(0), isExternal(false), isWritable(false), isUsedForWriting(false) {}
    QString url;
    QnUuid storageId;
    qint64 totalSpace;
    qint64 freeSpace;
    qint64 reservedSpace;
    bool isExternal;
    bool isWritable;
    bool isUsedForWriting;
    QString storageType;
};

#define QnStorageSpaceData_Fields (url)(storageId)(totalSpace)(freeSpace)(reservedSpace)(isExternal)(isWritable)(isUsedForWriting)(storageType)

struct QnStorageStatusReply {
    bool pluginExists;
    QnStorageSpaceData storage;
};

#define QnStorageStatusReply_Fields (pluginExists)(storage)

QN_FUSION_DECLARE_FUNCTIONS(QnStorageStatusReply, (json)(metatype))

#endif // QN_CHECK_STORAGE_REPLY_H
