#pragma once

#include <nx/fusion/model_functions_fwd.h>

#include <utils/common/request_param.h>

struct AnalyticsActionResult
{
    QString actionUrl;
    QString messageToUser;
};
#define AnalyticsActionResult_Fields (actionUrl)(messageToUser)

struct AnalyticsAction
{
    /** Id of a plugin which should handle the action. */
    QnUuid driverId;

    QString actionId;

    /** Id of a metadata object to which the action is applied. */
    QnUuid objectId;

    QnUuid cameraId;

    qint64 timestampUs;

    QMap<QString, QString> params;
};
#define AnalyticsAction_Fields (driverId)(actionId)(objectId)(cameraId)(timestampUs)(params)

struct AvailableAnalyticsActionsOfPlugin
{
    QnUuid driverId;
    QStringList actionIds;
};
#define AvailableAnalyticsActionsOfPlugin_Fields (driverId)(actionIds)

struct AvailableAnalyticsActions
{
    QList<AvailableAnalyticsActionsOfPlugin> actions;
};
#define AvailableAnalyticsActions_Fields (actions)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (AnalyticsActionResult)
    (AnalyticsAction)
    (AvailableAnalyticsActionsOfPlugin)
    (AvailableAnalyticsActions),
    (json));
