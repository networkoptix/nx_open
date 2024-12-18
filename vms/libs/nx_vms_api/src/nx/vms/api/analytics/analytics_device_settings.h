// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>
#include <optional>

#include <QtCore/QJsonObject>
#include <QtCore/QString>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/types/motion_types.h>

namespace nx::vms::api {

struct NX_VMS_API AnalyticsDeviceSettings
{
    /**%apidoc[immutable] */
    QString deviceId;

    /**%apidoc[immutable] */
    nx::Uuid engineId;

    /**%apidoc[opt]
     * Name-value map with setting values, using JSON types corresponding to each setting type.
     */
    QJsonObject values;

    /**%apidoc[opt] */
    QJsonObject model;

    /**%apidoc[opt]
     * Name-value map with errors that occurred while performing the current settings operation.
     */
    std::optional<std::map<QString, QString>> _error;

    /**%apidoc[opt]
     * Index of the stream that should be used for the analytics purposes.
     */
    StreamIndex analyzedStream = StreamIndex::undefined;

    /**%apidoc[opt]
     * Indicates whether the User is allowed to select which stream (primary or secondary) has to
     * be passed to the Plugin. If true, the analyzed stream is selected according to the Plugin
     * preferences (if any) or defaults to the primary stream.
     */
    bool disableStreamSelection = false;
};

#define AnalyticsDeviceSettings_Fields (deviceId)(engineId)(values) \
    (model)(_error)(analyzedStream)(disableStreamSelection)
QN_FUSION_DECLARE_FUNCTIONS(AnalyticsDeviceSettings, (json), NX_VMS_API);
NX_REFLECTION_INSTRUMENT(AnalyticsDeviceSettings, AnalyticsDeviceSettings_Fields);

} // namespace nx::vms::api
