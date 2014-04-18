#ifndef QN_STORAGE_SPACE_REPLY_H
#define QN_STORAGE_SPACE_REPLY_H

#include <QtCore/QString>
#include <QtCore/QVariant>
#include <QtCore/QMetaType>

#include <utils/serialization/json_fwd.h>

#include "storage_status_reply.h"

struct QnStorageSpaceReply {
    QList<QnStorageSpaceData> storages;
    QList<QString> storageProtocols;
};

Q_DECLARE_METATYPE(QnStorageSpaceReply);
QN_DECLARE_JSON_SERIALIZATION_FUNCTIONS(QnStorageSpaceReply)

#endif // QN_STORAGE_SPACE_DATA_H
