#ifndef QN_STORAGE_STATUS_REPLY_H
#define QN_STORAGE_STATUS_REPLY_H

#include <QtCore/QMetaType>

#include <utils/serialization/json_fwd.h>
#include <utils/common/id.h>

struct QnStorageSpaceData {
    QString path;
    QnId storageId;
    qint64 totalSpace;
    qint64 freeSpace;
    qint64 reservedSpace;
    bool isExternal;
    bool isWritable;
    bool isUsedForWriting;
};

struct QnStorageStatusReply {
    bool pluginExists;
    QnStorageSpaceData storage;
};

Q_DECLARE_METATYPE(QnStorageStatusReply)
QN_DECLARE_JSON_SERIALIZATION_FUNCTIONS(QnStorageSpaceData)
QN_DECLARE_JSON_SERIALIZATION_FUNCTIONS(QnStorageStatusReply)

#endif // QN_CHECK_STORAGE_REPLY_H
