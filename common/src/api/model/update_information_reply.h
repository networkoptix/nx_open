#pragma once

#include <QtCore/QHash>
#include <QtCore/QMetaType>

#include <nx/fusion/model_functions_fwd.h>
#include <utils/common/id.h>

struct QnUpdateFreeSpaceReply
{
    QHash<QnUuid, qint64> freeSpaceByServerId;
};

#define QnUpdateFreeSpaceReply_Fields (freeSpaceByServerId)

QN_FUSION_DECLARE_FUNCTIONS(QnUpdateFreeSpaceReply, (ubjson)(xml)(json)(csv_record)(metatype))
