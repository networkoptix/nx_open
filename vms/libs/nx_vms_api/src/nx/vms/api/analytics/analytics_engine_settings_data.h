// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QJsonObject>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/json/qjson.h>
#include <nx/utils/uuid.h>

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
     * Name-value map with errors that occurred while performing the current settings operation.
     */
    QJsonObject settingsErrors;

    bool operator==(const EngineSettingsResponse& other) const = default;
};
#define nx_vms_api_analytics_EngineSettingsResponse_Fields \
    (settingsValues) \
    (settingsModel) \
    (settingsErrors)
QN_FUSION_DECLARE_FUNCTIONS(EngineSettingsResponse, (json), NX_VMS_API)

struct NX_VMS_API EngineActiveSettingChangedResponse: EngineSettingsResponse
{
    /**%apidoc
     * If not empty, the Client will open this URL in an embedded browser.
     */
    QString actionUrl;

    /**%apidoc
     * If not empty, the Client will show this text to the user.
     */
    QString messageToUser;

    /**%apidoc
     * If true, the Client will use proxying through the engine parent server for actionUrl.
     */
    bool useProxy = false;

    /**%apidoc
     * If true, the Client will use device authentication for actionUrl.
     */
    bool useDeviceCredentials = false;

    bool operator==(const EngineActiveSettingChangedResponse& other) const = default;
};
#define nx_vms_api_analytics_EngineActiveSettingChangedResponse_Fields \
    nx_vms_api_analytics_EngineSettingsResponse_Fields \
    (actionUrl) \
    (messageToUser) \
    (useProxy) \
    (useDeviceCredentials)

QN_FUSION_DECLARE_FUNCTIONS(EngineActiveSettingChangedResponse, (json), NX_VMS_API)

struct NX_VMS_API EngineSettingsRequest
{
    /**%apidoc
     * Unique id of an Analytics Engine.
     */
    nx::Uuid analyticsEngineId;

    /**%apidoc
     * Name-value map with setting values, using JSON types corresponding to each setting type.
     */
    QJsonObject settingsValues;

    bool operator==(const EngineSettingsRequest& other) const = default;
};
#define nx_vms_api_analytics_EngineSettingsRequest_Fields \
    (analyticsEngineId) \
    (settingsValues)
QN_FUSION_DECLARE_FUNCTIONS(EngineSettingsRequest, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(EngineSettingsRequest, nx_vms_api_analytics_EngineSettingsRequest_Fields)

struct NX_VMS_API EngineActiveSettingChangedRequest
{
    /**%apidoc
     * Unique id of an Analytics Engine.
     */
    nx::Uuid analyticsEngineId;

    /**%apidoc
     * Name of a setting which triggered the notification.
     */
    QString activeSettingName;

    /**%apidoc
     * Settings model.
     */
    QJsonObject settingsModel;

    /**%apidoc
     * Name-value map with setting values, using JSON types corresponding to each setting type.
     */
    QJsonObject settingsValues;

    /**%apidoc
     * Name-value map with param values, using JSON types corresponding to each setting type.
     */
    QJsonObject paramValues;

    bool operator==(const EngineActiveSettingChangedRequest& other) const = default;
};
#define nx_vms_api_analytics_EngineActiveSettingChangedRequest_Fields \
    (analyticsEngineId) \
    (activeSettingName) \
    (settingsModel) \
    (settingsValues) \
    (paramValues)

QN_FUSION_DECLARE_FUNCTIONS(EngineActiveSettingChangedRequest, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(EngineActiveSettingChangedRequest,
    nx_vms_api_analytics_EngineActiveSettingChangedRequest_Fields)

} // namespace nx::vms::api::analytics
