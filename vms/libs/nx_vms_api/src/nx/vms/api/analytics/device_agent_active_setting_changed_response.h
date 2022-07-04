// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

#include <nx/fusion/model_functions_fwd.h>

#include "device_agent_settings_response.h"
#include "settings.h"

namespace nx::vms::api::analytics {

struct NX_VMS_API DeviceAgentActiveSettingChangedResponse: DeviceAgentSettingsResponse
{
    /**%apidoc
     * If not empty, the Client will open this URL in an embedded browser.
     */
    QString actionUrl;

    /**%apidoc
     * If not empty, the Client will show this text to the user.
     */
    QString messageToUser;
};
#define nx_vms_api_analytics_DeviceAgentActiveSettingChangedResponse_Fields \
    nx_vms_api_analytics_DeviceAgentSettingsResponse_Fields \
    (actionUrl) \
    (messageToUser)

QN_FUSION_DECLARE_FUNCTIONS(DeviceAgentActiveSettingChangedResponse, (json), NX_VMS_API)

} // namespace nx::vms::api::analytics
