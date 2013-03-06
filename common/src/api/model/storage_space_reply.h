#ifndef QN_STORAGE_SPACE_REPLY_H
#define QN_STORAGE_SPACE_REPLY_H

#include <QtCore/QString>
#include <QtCore/QVariant>
#include <QtCore/QMetaType>

#include <utils/common/json.h>

#include "storage_status_reply.h"

struct QnStorageSpaceReply {
    QList<QnStorageSpaceData> storages;
    QList<QString> storageProtocols;
};

QN_DEFINE_STRUCT_SERIALIZATION_FUNCTIONS(QnStorageSpaceReply, (storages)(storageProtocols), inline)

Q_DECLARE_METATYPE(QnStorageSpaceReply);

#endif // QN_STORAGE_SPACE_DATA_H
