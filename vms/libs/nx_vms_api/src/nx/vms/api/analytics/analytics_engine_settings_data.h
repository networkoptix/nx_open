#pragma once

#include <QtCore/QJsonObject>

#include <nx/utils/uuid.h>

#include <nx/fusion/model_functions_fwd.h>

namespace nx::vms::api::analytics {

struct NX_VMS_API EngineSettingsResponse
{
    /**%apidoc
     * Name-value map with setting values, using JSON types corresponding to each setting type.
     */
    QJsonObject settingsValues;

    /**%apidoc
     * Model of settings containing setting names, types and value restrictions.
     */
    QJsonObject settingsModel;

    /**%apidoc
     * Current id of the Settings Model. Such ids are used to check that values match the Model.
     */
    QnUuid settingsModelId;

    /**%apidoc
     * Name-value map with errors that occurred while performing the current settings operation.
     */
    QJsonObject settingsErrors;
};
#define nx_vms_api_analytics_EngineSettingsResponse_Fields \
    (settingsValues) \
    (settingsModel) \
    (settingsModelId) \
    (settingsErrors)


struct NX_VMS_API EngineSettingsRequest
{
    /**%apidoc
     * Unique id of an Analytics Engine.
     */
    QnUuid analyticsEngineId;

    /**%apidoc
     * Id of the Settings Model the values supposed to be applied to.
     */
    QnUuid settingsModelId;

    /**%apidoc
     * Name-value map with setting values, using JSON types corresponding to each setting type.
     */
    QJsonObject settingsValues;
};
#define nx_vms_api_analytics_EngineSettingsRequest_Fields \
    (analyticsEngineId) \
    (settingsModelId) \
    (settingsValues)

QN_FUSION_DECLARE_FUNCTIONS(EngineSettingsResponse, (json)(eq), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(EngineSettingsRequest, (json)(eq), NX_VMS_API)

} // namespace nx::vms::api::analytics
