#pragma once

#include <QtCore/QJsonObject>

#include <nx/utils/uuid.h>

#include <nx/fusion/model_functions_fwd.h>

namespace nx::vms::api::analytics {

NX_VMS_API struct EngineSettingsResponse
{
    QJsonObject settingsValues;
    QJsonObject settingsModel;
    QnUuid settingsModelId;
    QJsonObject settingsErrors;
};
#define nx_vms_api_analytics_EngineSettingsResponse_Fields \
    (settingsValues) \
    (settingsModel) \
    (settingsModelId) \
    (settingsErrors)

QN_FUSION_DECLARE_FUNCTIONS(EngineSettingsResponse, (json)(eq), NX_VMS_API)

} // namespace nx::vms::api::analytics
