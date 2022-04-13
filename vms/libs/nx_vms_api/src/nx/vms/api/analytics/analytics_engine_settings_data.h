// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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

    bool operator==(const EngineSettingsResponse& other) const = default;
};
#define nx_vms_api_analytics_EngineSettingsResponse_Fields \
    (settingsValues) \
    (settingsModel) \
    (settingsModelId) \
    (settingsErrors)
QN_FUSION_DECLARE_FUNCTIONS(EngineSettingsResponse, (json), NX_VMS_API)

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

    bool operator==(const EngineSettingsRequest& other) const = default;
};
#define nx_vms_api_analytics_EngineSettingsRequest_Fields \
    (analyticsEngineId) \
    (settingsModelId) \
    (settingsValues)
QN_FUSION_DECLARE_FUNCTIONS(EngineSettingsRequest, (json), NX_VMS_API)

struct NX_VMS_API EngineActiveSettingChangedRequest
{
    /**%apidoc
     * Unique id of an Analytics Engine.
     */
    QnUuid analyticsEngineId;

    /**%apidoc
     * Id of a setting which triggered the notification.
     */
    QString activeSettingId;

    /**%apidoc
     * Settings model.
     */
    QJsonObject settingsModel;

    /**%apidoc
     * Name-value map with setting values, using JSON types corresponding to each setting type.
     */
    QJsonObject settingsValues;

    bool operator==(const EngineActiveSettingChangedRequest& other) const = default;
};
#define nx_vms_api_analytics_EngineActiveSettingChangedRequest_Fields \
    (analyticsEngineId) \
    (activeSettingId) \
    (settingsModel) \
    (settingsValues)

QN_FUSION_DECLARE_FUNCTIONS(EngineActiveSettingChangedRequest, (json), NX_VMS_API)

} // namespace nx::vms::api::analytics
