// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <nx/reflect/enum_instrument.h>
#include <nx/utils/url.h>
#include <nx/vms/api/json/value_or_array.h>

#include "module_information.h"
#include "rest_api_versions.h"

namespace nx::vms::api {

NX_REFLECTION_ENUM_CLASS(SystemCompatibilityStatus,
    compatible,
    incompatible,
    inaccessible
)

struct NX_VMS_API SystemInformation
{
    QString name;
    QString customization;
    nx::utils::SoftwareVersion version; /**<%apidoc:string */
    int protoVersion = 0;
    RestApiVersions restApiVersions;
    QString cloudHost;

    /**%apidoc If not present, the System is new. */
    std::optional<nx::Uuid> localId;

    /**%apidoc If present, the System is bound to the Cloud. */
    std::optional<QString> cloudId;

    /**%apidoc Presented if the System is bound to the Cloud. */
    std::optional<nx::Uuid> cloudOwnerId;

    /**%apidoc Presented if the System is bound to the Cloud and belongs to the Organization. */
    std::optional<nx::Uuid> organizationId;

    std::optional<std::vector<nx::Uuid>> servers;

    /**%apidoc[opt] Amount of edge Servers in the System. */
    int edgeServerCount = 0;

    std::optional<std::vector<nx::Uuid>> devices;

    std::optional<QString> endpoint; //< TODO: SocketAddress?

    std::optional<SystemCompatibilityStatus> status;

    /**%apidoc[proprietary] Internal identification of LDAP connection. */
    std::optional<QString> ldapSyncId;

    /**%apidoc Synchronized time of the VMS System, in milliseconds since epoch. */
    std::chrono::milliseconds synchronizedTimeMs{0};

    SystemInformation() = default;
    SystemInformation(const ModuleInformation& module);

    SystemInformation(const SystemInformation&) = default;
    SystemInformation(SystemInformation&&) = default;

    SystemInformation& operator=(const SystemInformation&) = default;
    SystemInformation& operator=(SystemInformation&&) = default;
};
#define SystemInformation_Fields \
    (name)(customization)(version)(protoVersion)(restApiVersions)(cloudHost)(localId)(cloudId) \
    (cloudOwnerId) (organizationId)(endpoint)(servers)(edgeServerCount)(devices)(status) \
    (ldapSyncId)(synchronizedTimeMs)
NX_VMS_API_DECLARE_STRUCT_EX(SystemInformation, (json))

struct NX_VMS_API OtherSystemRequest
{
    /**%apidoc[opt] Whether discovered Systems should be in the list. */
    bool showDiscovered = true;

    /**%apidoc:stringArray[opt] Endpoint to ping and add into the result. */
    std::optional<nx::vms::api::json::ValueOrArray<QString>> endpoint;
};
#define OtherSystemRequest_Fields (showDiscovered)(endpoint)
NX_VMS_API_DECLARE_STRUCT_EX(OtherSystemRequest, (json))

struct NX_VMS_API LocalSystemAuth
{
    /**%apidoc New "admin" user password. */
    QString password;

    /**%apidoc User agent of the client. */
    std::optional<QString> userAgent;
};
#define LocalSystemAuth_Fields (password)(userAgent)
NX_VMS_API_DECLARE_STRUCT_EX(LocalSystemAuth, (json))
NX_REFLECTION_INSTRUMENT(LocalSystemAuth, LocalSystemAuth_Fields)

struct NX_VMS_API CloudSystemAuth
{
    /**%apidoc The system Id given by cloud. */
    QString systemId;

    /**%apidoc The system authentication key given by the cloud. */
    QString authKey;

    /**%apidoc The cloud user, registered this system on the Cloud. */
    QString owner;

    /**%apidoc User agent of the client. */
    std::optional<QString> userAgent;

    /**%apidoc[opt] The cloud user's organization. */
    QString organizationId;
};
#define CloudSystemAuth_Fields (systemId)(authKey)(owner)(userAgent)(organizationId)
NX_VMS_API_DECLARE_STRUCT_EX(CloudSystemAuth, (json))
NX_REFLECTION_INSTRUMENT(CloudSystemAuth, CloudSystemAuth_Fields)

NX_REFLECTION_ENUM_CLASS(SystemSettingsPreset,
    compatibility,
    recommended,
    security
)

struct NX_VMS_API SetupSystemData
{
    /**%apidoc New System name.
     * %example System 1
     */
    QString name;

    /**%apidoc[opt] System settings preset. */
    SystemSettingsPreset settingsPreset = SystemSettingsPreset::recommended;

    /**%apidoc System settings to set when System is configured. */
    std::map<QString, QJsonValue> settings;

    /**%apidoc For local System only. */
    std::optional<LocalSystemAuth> local;

    /**%apidoc For cloud System only. */
    std::optional<CloudSystemAuth> cloud;
};
#define SetupSystemData_Fields (name)(settingsPreset)(settings)(local)(cloud)
NX_VMS_API_DECLARE_STRUCT_EX(SetupSystemData, (json))

} // namespace nx::vms::api
