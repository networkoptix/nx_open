// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QJsonObject>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>
#include <nx/utils/serialization/qjson.h>
#include <nx/vms/api/types/motion_types.h>

#include "settings.h"

namespace nx::vms::api::analytics {

struct NX_VMS_API DeviceAgentSettingsResponse
{
    /**%apidoc
     * Index of the stream that should be used for the analytics purposes.
     */
    StreamIndex analyzedStreamIndex = StreamIndex::undefined;

    /**%apidoc
     * Indicates whether the User is allowed to select which stream (primary or secondary) has to
     * be passed to the Plugin. If true, the analyzed stream is selected according to the Plugin
     * preferences (if any) or defaults to the primary stream.
     */
    bool disableStreamSelection = false;

    /**%apidoc
     * Name-value map with setting values, using JSON types corresponding to each setting type.
     */
    SettingsValues settingsValues;

    /**%apidoc
     * Model of settings containing setting names, types and value restrictions.
     */
    SettingsModel settingsModel;

    /**%apidoc
     * Name-value map with errors that occurred while performing the current settings operation.
     */
    QJsonObject settingsErrors;
};
#define nx_vms_api_analytics_DeviceAgentSettingsResponse_Fields \
    (analyzedStreamIndex) \
    (disableStreamSelection) \
    (settingsValues) \
    (settingsModel) \
    (settingsErrors)

QN_FUSION_DECLARE_FUNCTIONS(DeviceAgentSettingsResponse, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(DeviceAgentSettingsResponse,
    nx_vms_api_analytics_DeviceAgentSettingsResponse_Fields)

} // namespace nx::vms::api::analytics
