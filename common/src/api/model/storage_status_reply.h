#ifndef QN_STORAGE_STATUS_REPLY_H
#define QN_STORAGE_STATUS_REPLY_H

#include <QtCore/QMetaType>

#include <utils/common/json.h>

struct QnStorageSpaceData {
    QString path;
    int storageId;
    qint64 totalSpace;
    qint64 freeSpace;
    qint64 reservedSpace;
    bool isWritable;
    bool isUsedForWriting;
};

struct QnStorageStatusReply {
    bool pluginExists;
    QnStorageSpaceData storage;
};

QN_DEFINE_STRUCT_SERIALIZATION_FUNCTIONS(QnStorageSpaceData, (path)(storageId)(totalSpace)(freeSpace)(reservedSpace)(isWritable)(isUsedForWriting), inline)
QN_DEFINE_STRUCT_SERIALIZATION_FUNCTIONS(QnStorageStatusReply, (pluginExists)(storage), inline)

Q_DECLARE_METATYPE(QnStorageStatusReply)

#endif // QN_CHECK_STORAGE_REPLY_H
