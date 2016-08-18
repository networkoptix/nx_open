#pragma once

#include <QtCore/QMetaType>

#include <nx/fusion/model_functions_fwd.h>
#include <utils/common/id.h>

struct QnUpdateFreeSpaceReply
{
    QnUuid serverId;
    qint64 freeSpace = 0;

    QnUpdateFreeSpaceReply() {}

    QnUpdateFreeSpaceReply(const QnUuid& serverId, qint64 freeSpace):
        serverId(serverId),
        freeSpace(freeSpace)
    {}
};

using QnMultiServerUpdateFreeSpaceReply = QList<QnUpdateFreeSpaceReply>;

#define QnUpdateFreeSpaceReply_Fields (serverId)(freeSpace)

QN_FUSION_DECLARE_FUNCTIONS(QnUpdateFreeSpaceReply, (json)(metatype))
