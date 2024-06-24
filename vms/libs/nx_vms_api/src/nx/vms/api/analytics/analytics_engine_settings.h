// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>

#include <QtCore/QJsonObject>
#include <QtCore/QString>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>

namespace nx::vms::api {

struct NX_VMS_API AnalyticsEngineSettings
{
    /**%apidoc[immutable]
     * Id of an Analytics Engine.
     */
    nx::Uuid id;

    /**%apidoc[opt]
     * Name-value map with setting values, using json types corresponding to each setting type.
     * If not specified, current setting values will be returned.
     */
    QJsonObject values;

    /**%apidoc[readonly]
     * Name-value map with errors that occurred while performing the current settings operation.
     */
    std::optional<std::map<QString, QString>> _error;
};
#define AnalyticsEngineSettings_Fields (id)(values)(_error)
QN_FUSION_DECLARE_FUNCTIONS(AnalyticsEngineSettings, (json), NX_VMS_API);
NX_REFLECTION_INSTRUMENT(AnalyticsEngineSettings, AnalyticsEngineSettings_Fields);

} // namespace nx::vms::api
