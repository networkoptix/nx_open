#ifndef QN_STORAGE_SPACE_REPLY_H
#define QN_STORAGE_SPACE_REPLY_H

#include <QtCore/QString>
#include <QtCore/QVariant>
#include <QtCore/QMetaType>

#include <utils/common/model_functions_fwd.h>

#include "storage_status_reply.h"

struct QnStorageSpaceReply {
    QList<QnStorageSpaceData> storages;
    QList<QString> storageProtocols;
};

#define QnStorageSpaceReply_Fields (storages)(storageProtocols)

QN_FUSION_DECLARE_FUNCTIONS(QnStorageSpaceReply, (json)(metatype))

#endif // QN_STORAGE_SPACE_DATA_H
