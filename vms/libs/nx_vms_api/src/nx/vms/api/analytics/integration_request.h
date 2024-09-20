// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <optional>

#include <QtCore/QJsonValue>
#include <QtCore/QString>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/fusion/serialization/json_functions.h>
#include <nx/fusion/serialization/json_fwd.h>
#include <nx/reflect/instrument.h>
#include <nx/reflect/json.h>
#include <nx/utils/serialization/qjson.h>
#include <nx/utils/serialization/qt_core_types.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/analytics/device_agent_manifest.h>
#include <nx/vms/api/analytics/engine_manifest.h>
#include <nx/vms/api/analytics/plugin_manifest.h>

namespace nx::vms::api::analytics {

struct NX_VMS_API IntegrationRequestIdentity
{
    /**%apidoc[readonly] Integration request id. */
    nx::Uuid requestId;

    IntegrationRequestIdentity() = default;
    IntegrationRequestIdentity(const nx::Uuid& requestId): requestId(requestId) {}

    nx::Uuid getId() const { return requestId; }

    bool operator==(const IntegrationRequestIdentity& other) const = default;
};
#define nx_vms_api_analytics_IntegrationRequestIdentity_Fields (requestId)
QN_FUSION_DECLARE_FUNCTIONS(
    IntegrationRequestIdentity, (json), NX_VMS_API);
NX_REFLECTION_INSTRUMENT(
    IntegrationRequestIdentity,
    nx_vms_api_analytics_IntegrationRequestIdentity_Fields);

/**%apidoc
 * Data to create/update an Integration request in the VMS.
 */
struct NX_VMS_API RegisterIntegrationRequest: public IntegrationRequestIdentity
{
    /**%apidoc Integration Manifest. */
    PluginManifest integrationManifest;

    /**%apidoc Engine Manifest. */
    EngineManifest engineManifest;

    /**%apidoc Integration authentication code, 4 decimal digits. */
    QString pinCode;

    /**%apidoc Whether the Integration is REST-only. */
    bool isRestOnly = false;

    /**%apidoc Device Agent Manifest should be provided by REST-only Integrations. */
    std::optional<DeviceAgentManifest> deviceAgentManifest;

    bool operator==(const RegisterIntegrationRequest& other) const = default;
};
#define nx_vms_api_analytics_RegisterIntegrationRequest_Fields \
    nx_vms_api_analytics_IntegrationRequestIdentity_Fields \
    (integrationManifest) \
    (engineManifest) \
    (pinCode) \
    (isRestOnly) \
    (deviceAgentManifest)
QN_FUSION_DECLARE_FUNCTIONS(
    RegisterIntegrationRequest, (json), NX_VMS_API);
NX_REFLECTION_INSTRUMENT(
    RegisterIntegrationRequest,
    nx_vms_api_analytics_RegisterIntegrationRequest_Fields);

/**%apidoc
 * Data to be returned after creating an Integration request.
 */
struct NX_VMS_API RegisterIntegrationResponse: public IntegrationRequestIdentity
{
    /**%apidoc
     * Username of the user, which should be used by the Integration to log in to the VMS.
     */
    QString user;

    /**%apidoc
     * Password of the user, which should be used by the Integration to log in to the VMS.
     */
    QString password;
};
#define nx_vms_api_analytics_RegisterIntegrationResponse_Fields \
    nx_vms_api_analytics_IntegrationRequestIdentity_Fields \
    (user) \
    (password)
QN_FUSION_DECLARE_FUNCTIONS(
    RegisterIntegrationResponse, (json), NX_VMS_API);
NX_REFLECTION_INSTRUMENT(
    RegisterIntegrationResponse,
    nx_vms_api_analytics_RegisterIntegrationResponse_Fields);

struct NX_VMS_API IntegrationRequestData: public RegisterIntegrationRequest
{
    IntegrationRequestData() = default;
    IntegrationRequestData(RegisterIntegrationRequest registerIntegrationRequest):
        RegisterIntegrationRequest(std::move(registerIntegrationRequest))
    {
    }

    /**%apidoc[readonly] Timestamp of the Integration request creation or last update. */
    std::chrono::milliseconds timestampMs{};

    /**%apidoc[readonly] IP address the request was received from. */
    QString requestAddress;

    /**%apidoc[readonly] Whether the request is approved or not. */
    bool isApproved = false;

     /**%apidoc[readonly] Integration id. Set when the Integration request is approved. */
    std::optional<nx::Uuid> integrationId;

    bool operator==(const IntegrationRequestData& other) const = default;
};
#define nx_vms_api_analytics_IntegrationRequestData_Fields \
    nx_vms_api_analytics_RegisterIntegrationRequest_Fields \
    (timestampMs)(requestAddress)(isApproved)(integrationId)
QN_FUSION_DECLARE_FUNCTIONS(
    IntegrationRequestData, (json), NX_VMS_API);
NX_REFLECTION_INSTRUMENT(
    IntegrationRequestData,
    nx_vms_api_analytics_IntegrationRequestData_Fields);

} // namespace nx::vms::api::analytics
