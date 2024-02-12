// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <QtCore/QString>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/serialization/qt_core_types.h>
#include <nx/utils/uuid.h>

namespace nx::vms::api::analytics {

struct NX_VMS_API IntegrationRequestIdentity
{
    /**%apidoc Integration request id. */
    nx::Uuid requestId;

    IntegrationRequestIdentity() = default;
    IntegrationRequestIdentity(const nx::Uuid& requestId): requestId(requestId) {}

    nx::Uuid getId() { return requestId; }

    bool operator==(const IntegrationRequestIdentity& other) const = default;
};
#define nx_vms_api_analytics_IntegrationRequestIdentity_Fields (requestId)
QN_FUSION_DECLARE_FUNCTIONS(
    IntegrationRequestIdentity, (json)(ubjson), NX_VMS_API);
NX_REFLECTION_INSTRUMENT(
    IntegrationRequestIdentity,
    nx_vms_api_analytics_IntegrationRequestIdentity_Fields);

/**%apidoc
 * Data to create/update an Integration request in the VMS.
 */
struct NX_VMS_API RegisterIntegrationRequest: public IntegrationRequestIdentity
{
    /**%apidoc Integration id. */
    QString id;

    /**%apidoc Integration name. */
    QString name;

    /**%apidoc Integration description. */
    QString description;

    /**%apidoc Integration version. */
    QString version;

    /**%apidoc Integration vendor. */
    QString vendor;

    /**%apidoc Integration authentication code, 4 decimal digits. */
    QString pinCode;

    bool operator==(const RegisterIntegrationRequest& other) const = default;
};
#define nx_vms_api_analytics_RegisterIntegrationRequest_Fields \
    nx_vms_api_analytics_IntegrationRequestIdentity_Fields \
    (id) \
    (name) \
    (description) \
    (version) \
    (vendor) \
    (pinCode)
QN_FUSION_DECLARE_FUNCTIONS(
    RegisterIntegrationRequest, (json)(ubjson), NX_VMS_API);
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
    RegisterIntegrationResponse, (json)(ubjson), NX_VMS_API);
NX_REFLECTION_INSTRUMENT(
    RegisterIntegrationResponse,
    nx_vms_api_analytics_RegisterIntegrationResponse_Fields);

/**%apidoc
 * Inforamtion about an Integration request.
 */
struct NX_VMS_API IntegrationRequestData: public RegisterIntegrationRequest
{
    IntegrationRequestData() = default;
    IntegrationRequestData(RegisterIntegrationRequest registerIntegrationRequest):
        RegisterIntegrationRequest(std::move(registerIntegrationRequest))
    {
    }

    /**%apidoc Timestamp of the Integration request creation or last update. */
    std::chrono::milliseconds timestampMs{};

    /**%apidoc Integration request expiration timeout in milliseconds. */
    std::chrono::milliseconds expirationTimeoutMs{};

    /**%apidoc IP address the request was received from. */
    QString requestAddress;

    /**%apidoc Whether the request is approved or not. */
    bool isApproved = false;

    bool operator==(const IntegrationRequestData& other) const = default;
};
#define nx_vms_api_analytics_IntegrationRequestData_Fields \
    nx_vms_api_analytics_RegisterIntegrationRequest_Fields \
    (timestampMs)(expirationTimeoutMs)(requestAddress)(isApproved)
QN_FUSION_DECLARE_FUNCTIONS(
    IntegrationRequestData, (json)(ubjson), NX_VMS_API);
NX_REFLECTION_INSTRUMENT(
    IntegrationRequestData,
    nx_vms_api_analytics_IntegrationRequestData_Fields);

} // namespace nx::vms::api::analytics
