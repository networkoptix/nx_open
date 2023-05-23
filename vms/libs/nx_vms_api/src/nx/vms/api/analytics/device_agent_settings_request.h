// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QJsonObject>

#include <nx/utils/uuid.h>

#include <nx/fusion/model_functions_fwd.h>

#include <nx/vms/api/types/motion_types.h>

#include "settings.h"

namespace nx::vms::api::analytics {

struct NX_VMS_API DeviceAgentSettingsRequest
{
    /**%apidoc
     * Id of Device.
     */
    QString deviceId;

    /**%apidoc
     * Unique id of an Analytics Engine.
     */
    QnUuid analyticsEngineId;

    /**%apidoc
     * Index of the stream that should be used for the analytics purposes.
     */
    StreamIndex analyzedStreamIndex = StreamIndex::undefined;

    /**%apidoc
     * Name-value map with setting values, using JSON types corresponding to each setting type.
     */
    SettingsValues settingsValues;

    /**%apidoc
     * Id of the Settings Model the values supposed to be applied to.
     */
    QnUuid settingsModelId;
};
#define nx_vms_api_analytics_DeviceAgentSettingsRequest_Fields \
    (deviceId) \
    (analyticsEngineId) \
    (analyzedStreamIndex) \
    (settingsValues) \
    (settingsModelId)

QN_FUSION_DECLARE_FUNCTIONS(DeviceAgentSettingsRequest, (json), NX_VMS_API)

} // namespace nx::vms::api::analytics
