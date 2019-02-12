#pragma once

#include <QtCore/QJsonObject>

#include <nx/fusion/model_functions_fwd.h>

namespace nx::vms::api::analytics {

struct SettingsResponse
{
    QJsonObject values;
    QJsonObject model;
};

#define nx_vms_api_analytics_SettingsResponse_Fields (values)(model)

QN_FUSION_DECLARE_FUNCTIONS(SettingsResponse, (json)(eq), NX_VMS_API)

} // namespace nx::vms::api::analytics
