// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_STORAGE_SPACE_REPLY_H
#define QN_STORAGE_SPACE_REPLY_H

#include <QtCore/QString>
#include <QtCore/QVariant>

#include <nx/fusion/model_functions_fwd.h>

#include "storage_status_reply.h"

struct QnStorageSpaceReply
{
    nx::vms::api::StorageSpaceDataList storages;
    QList<QString> storageProtocols;
};

#define QnStorageSpaceReply_Fields (storages)(storageProtocols)

QN_FUSION_DECLARE_FUNCTIONS(QnStorageSpaceReply, (json), NX_VMS_COMMON_API)

#endif // QN_STORAGE_SPACE_DATA_H
