// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <analytics/db/analytics_db_types.h>
#include <nx/fusion/model_functions_fwd.h>

/**
 * NOTE: Corresponds to struct IAction::Result.
 */
struct NX_VMS_COMMON_API AnalyticsActionResult
{
    /**%apidoc
     * URL to be opened by the Client in an embedded browser, or a null or empty string. If
     * non-empty, messageToUser must be null or empty.
     */
    QString actionUrl;

    /**%apidoc
     * Text to be shown to the user by the Client, or a null or empty string. If non-empty,
     * actionUrl must be null or empty.
     */
    QString messageToUser;

    /**%apidoc
     * Whether proxying through the connected server should be used for actionUrl.
     */
    bool useProxy = false;

    /**%apidoc
     * Whether device authentication should be used for actionUrl.
     */
    bool useDeviceCredentials = false;
};
#define AnalyticsActionResult_Fields (actionUrl)(messageToUser)(useProxy)(useDeviceCredentials)

struct NX_VMS_COMMON_API AnalyticsAction
{
    /** Id of an Engine which should handle the Action. */
    nx::Uuid engineId;

    QString actionId;

    /** Id of an Analytics Object Track to which the Action is applied. */
    nx::Uuid objectTrackId;

    nx::Uuid deviceId;

    qint64 timestampUs;

    using Parameters = QMap<QString, QString>;
    Parameters params;
};
#define AnalyticsAction_Fields (engineId)(actionId)(objectTrackId)(deviceId)(timestampUs)(params)

struct NX_VMS_COMMON_API AvailableAnalyticsActionsOfEngine
{
    /**%apidoc Id of the Analytics Engine which offers the Actions. */
    nx::Uuid engineId;

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
