#pragma once

#include <QtCore/QJsonObject>

#include <nx/utils/uuid.h>

#include <nx/fusion/model_functions_fwd.h>

#include <nx/vms/api/types/motion_types.h>

namespace nx::vms::api::analytics {

struct NX_VMS_API DeviceAnalyticsSettingsRequest
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
     * Index of the stream that should be used for analytics purposes.
     */
    nx::vms::api::StreamIndex analyzedStreamIndex = nx::vms::api::StreamIndex::undefined;

    /**%apidoc
     * Name-value map with setting values, using JSON types corresponding to each setting type.
     */
    QJsonObject settingsValues;

    QnUuid settingsModelId;
};
#define nx_vms_api_analytics_DeviceAnalyticsSettingsRequest_Fields \
    (deviceId) \
    (analyticsEngineId) \
    (analyzedStreamIndex) \
    (settingsValues) \
    (settingsModelId)

QN_FUSION_DECLARE_FUNCTIONS(DeviceAnalyticsSettingsRequest, (json)(eq), NX_VMS_API)

struct NX_VMS_API DeviceAnalyticsSettingsResponse
{
    /**%apidoc
     * Index of the stream that should be used for analytics purposes.
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
    QJsonObject settingsValues;

    /**%apidoc
     * Model of settings containing setting names, types and value restrictions.
     */
    QJsonObject settingsModel;

    QJsonObject settingsErrors;

    QnUuid settingsModelId;
};
#define nx_vms_api_analytics_DeviceAnalyticsSettingsResponse_Fields \
    (analyzedStreamIndex) \
    (disableStreamSelection) \
    (settingsValues) \
    (settingsModel) \
    (settingsErrors) \
    (settingsModelId)

QN_FUSION_DECLARE_FUNCTIONS(DeviceAnalyticsSettingsResponse, (json)(eq), NX_VMS_API)

} // namespace nx::vms::api::analytics
