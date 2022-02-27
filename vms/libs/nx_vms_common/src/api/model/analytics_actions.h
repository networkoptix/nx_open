// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <nx/fusion/model_functions_fwd.h>

#include <analytics/db/analytics_db_types.h>

struct NX_VMS_COMMON_API AnalyticsActionResult
{
    QString actionUrl;
    QString messageToUser;
};
#define AnalyticsActionResult_Fields (actionUrl)(messageToUser)

struct NX_VMS_COMMON_API AnalyticsAction
{
    /** Id of an Engine which should handle the Action. */
    QnUuid engineId;

    QString actionId;

    /** Id of an Analytics Object Track to which the Action is applied. */
    QnUuid objectTrackId;

    QnUuid deviceId;

    qint64 timestampUs;

    using Parameters = QMap<QString, QString>;
    Parameters params;
};
#define AnalyticsAction_Fields (engineId)(actionId)(objectTrackId)(deviceId)(timestampUs)(params)

struct NX_VMS_COMMON_API AvailableAnalyticsActionsOfEngine
{
    QnUuid engineId;

    QStringList actionIds;
};
#define AvailableAnalyticsActionsOfEngine_Fields (engineId)(actionIds)

struct NX_VMS_COMMON_API AvailableAnalyticsActions
{
    QList<AvailableAnalyticsActionsOfEngine> actions;
};
#define AvailableAnalyticsActions_Fields (actions)

QN_FUSION_DECLARE_FUNCTIONS(AnalyticsActionResult, (json), NX_VMS_COMMON_API)
QN_FUSION_DECLARE_FUNCTIONS(AnalyticsAction, (json), NX_VMS_COMMON_API)
QN_FUSION_DECLARE_FUNCTIONS(AvailableAnalyticsActionsOfEngine, (json), NX_VMS_COMMON_API)
QN_FUSION_DECLARE_FUNCTIONS(AvailableAnalyticsActions, (json), NX_VMS_COMMON_API)
