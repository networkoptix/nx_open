// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <map>
#include <vector>

#include <QtCore/QString>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/serialization/qt_core_types.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/json/value_or_array.h>

namespace nx::vms::api {

struct NX_VMS_API RequestAvailableAnalyticsActions
{
    /**%apidoc[opt]
     * Server on which the Analytics Actions are applicable to the specified metadata Object type.
     * If not set, non-local engines will be included in the search results.
     */
    nx::Uuid serverId;

    /**%apidoc[opt]:stringArray
     * Engine id(s) to get Analytic Actions for.
     */
    nx::vms::api::json::ValueOrArray<QString> engineId;

    /**%apidoc
     * Id of an Object type to which an Action should be applicable.
     */
    QString objectTypeId;
};
#define RequestAvailableAnalyticsActions_Fields (serverId)(engineId)(objectTypeId)
QN_FUSION_DECLARE_FUNCTIONS(RequestAvailableAnalyticsActions, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(
    RequestAvailableAnalyticsActions, RequestAvailableAnalyticsActions_Fields);

struct NX_VMS_API AvailableAnalyticsActions
{
    /**%apidoc[readonly] */
    std::vector<QString> actionIds;
};
#define AvailableAnalyticsActions_Fields (actionIds)
QN_FUSION_DECLARE_FUNCTIONS(AvailableAnalyticsActions, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(AvailableAnalyticsActions, AvailableAnalyticsActions_Fields);

using EngineActionsMap = std::map<nx::Uuid /*engineId*/, AvailableAnalyticsActions>;

struct NX_VMS_API AnalyticsAction
{
    /**%apidoc
     * Id of an Engine which should handle the Action.
     */
    nx::Uuid engineId;

    QString actionId;

    /**%apidoc
     * Id of an Object Track to which the Action is applied.
     */
    nx::Uuid objectTrackId;

    /**%apidoc
     * Flexible id of a Device to which the Action is applied.
     */
    QString deviceId;

    /**%apidoc
     * Timestamp of the video frame from which the Action was triggered, in microseconds.
     * %example 0
     */
    std::chrono::microseconds timestampUs{};

    /**%apidoc
     * JSON object with key-value pairs containing values for the Action parameters described in the Engine manifest.
     */
    std::map<QString, QString> parameters;

    static DeprecatedFieldNames const * getDeprecatedFieldNames();
};
#define AnalyticsAction_Fields (engineId)(actionId)(objectTrackId)(deviceId)(timestampUs)(parameters)
QN_FUSION_DECLARE_FUNCTIONS(AnalyticsAction, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(AnalyticsAction, AnalyticsAction_Fields);

/**
 * NOTE: Corresponds to struct IAction::Result.
 */
struct NX_VMS_API AnalyticsActionResult
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
QN_FUSION_DECLARE_FUNCTIONS(AnalyticsActionResult, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(AnalyticsActionResult, AnalyticsActionResult_Fields);

}
