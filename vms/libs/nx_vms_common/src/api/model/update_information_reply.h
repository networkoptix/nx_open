// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QHash>

#include <nx/fusion/model_functions_fwd.h>
#include <utils/common/id.h>

struct QnUpdateFreeSpaceReply
{
    QHash<nx::Uuid, qint64> freeSpaceByServerId;
};

#define QnUpdateFreeSpaceReply_Fields (freeSpaceByServerId)

struct QnCloudHostCheckReply
{
    QString cloudHost;
    QList<nx::Uuid> failedServers;
};

#define QnCloudHostCheckReply_Fields (cloudHost)(failedServers)

QN_FUSION_DECLARE_FUNCTIONS(QnUpdateFreeSpaceReply,
    (ubjson)(xml)(json)(csv_record),
    NX_VMS_COMMON_API)
QN_FUSION_DECLARE_FUNCTIONS(QnCloudHostCheckReply,
    (ubjson)(xml)(json)(csv_record),
    NX_VMS_COMMON_API)
