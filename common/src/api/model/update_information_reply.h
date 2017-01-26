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

struct QnCloudHostCheckReply
{
    QString cloudHost;
    QList<QnUuid> failedServers;
};

#define QnCloudHostCheckReply_Fields (cloudHost)(failedServers)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (QnUpdateFreeSpaceReply)(QnCloudHostCheckReply),
    (ubjson)(xml)(json)(csv_record)(metatype))
