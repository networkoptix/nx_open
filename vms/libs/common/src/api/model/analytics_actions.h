#pragma once

#include <optional>

#include <nx/fusion/model_functions_fwd.h>

#include <utils/common/request_param.h>

#include <analytics/db/analytics_db_types.h>

struct AnalyticsActionResult
{
    QString actionUrl;
    QString messageToUser;
};
#define AnalyticsActionResult_Fields (actionUrl)(messageToUser)

struct AnalyticsAction
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

struct AvailableAnalyticsActionsOfEngine
{
    QnUuid engineId;

    QStringList actionIds;
};
#define AvailableAnalyticsActionsOfEngine_Fields (engineId)(actionIds)

struct AvailableAnalyticsActions
{
    QList<AvailableAnalyticsActionsOfEngine> actions;
};
#define AvailableAnalyticsActions_Fields (actions)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (AnalyticsActionResult)
    (AnalyticsAction)
    (AvailableAnalyticsActionsOfEngine)
    (AvailableAnalyticsActions),
    (json));
