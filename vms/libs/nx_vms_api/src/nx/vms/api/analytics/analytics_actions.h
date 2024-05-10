// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QJsonValue>
#include <QtCore/QString>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/fusion/serialization/json_fwd.h>
#include <nx/fusion/serialization/json_functions.h>
#include <nx/reflect/instrument.h>
#include <nx/reflect/json.h>
#include <nx/utils/serialization/qjson.h>
#include <nx/utils/serialization/qt_core_types.h>
#include <nx/utils/uuid.h>

namespace nx::vms::api {

struct NX_VMS_API AnalyticsAction
{
    /** Id of an Engine which should handle the Action. */
    nx::Uuid engineId;

    QString actionId;

    /** Id of an Analytics Object Track to which the Action is applied. */
    nx::Uuid objectTrackId;

    nx::Uuid deviceId;

    // todo: #skolesnik What kind of seconds is it?
    qint64 timestampUs{};

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
