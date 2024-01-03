// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <nx/reflect/enum_instrument.h>
#include <nx/utils/url.h>
#include <nx/vms/api/json/value_or_array.h>

#include "module_information.h"

namespace nx::vms::api {

NX_REFLECTION_ENUM_CLASS(SiteCompatibilityStatus,
    compatible,
    incompatible,
    inaccessible
)

struct NX_VMS_API SiteInformation
{
    QString name;
    QString customization;
    nx::utils::SoftwareVersion version; /**<%apidoc:string */
    int protoVersion = 0;
    QString cloudHost;

    /**%apidoc If not present, the Site is new. */
    std::optional<QnUuid> localId;

    /**%apidoc If present, the Site is bound to the Cloud. */
    std::optional<QString> cloudId;

    /**%apidoc Presented if the Site is bound to the Cloud. */
    std::optional<QnUuid> cloudOwnerId;

    /**%apidoc Presented if the Site is bound to the Cloud and belongs to the Organization. */
    std::optional<QnUuid> organizationId;

    /**%apidoc For local Site only. */
    std::optional<std::vector<QnUuid>> servers;

    /**%apidoc Amount of edge Servers in the Site. */
    int edgeServerCount = 0;

    /**%apidoc For local Site only. */
    std::optional<std::vector<QnUuid>> devices;

    /**%apidoc For remote Sites only. */
    std::optional<QString> endpoint; //< TODO: SocketAddress?

    /**%apidoc For remote Sites only. */
    std::optional<SiteCompatibilityStatus> status;

    /**%apidoc[proprietary] Internal identification of LDAP connection. */
    std::optional<QString> ldapSyncId;

    /**%apidoc Synchronized time of the VMS Site, in milliseconds since epoch. */
    std::chrono::milliseconds synchronizedTimeMs{0};

    SiteInformation() = default;
    SiteInformation(const ModuleInformation& module);

    SiteInformation(const SiteInformation&) = default;
    SiteInformation(SiteInformation&&) = default;

    SiteInformation& operator=(const SiteInformation&) = default;
    SiteInformation& operator=(SiteInformation&&) = default;
};
#define SiteInformation_Fields \
    (name)(customization)(version)(protoVersion)(cloudHost)(localId)(cloudId)(cloudOwnerId) \
    (organizationId)(endpoint)(servers)(edgeServerCount)(devices)(status)(ldapSyncId) \
    (synchronizedTimeMs)
NX_VMS_API_DECLARE_STRUCT_EX(SiteInformation, (json))

struct NX_VMS_API OtherSiteRequest
{
    /**%apidoc[opt] Whether discovered Sites should be in the list. */
    bool showDiscovered = true;

    /**%apidoc:stringArray[opt] Endpoint to ping and add into the result. */
    std::optional<nx::vms::api::json::ValueOrArray<QString>> endpoint;
};
#define OtherSiteRequest_Fields (showDiscovered)(endpoint)
NX_VMS_API_DECLARE_STRUCT_EX(OtherSiteRequest, (json))

struct NX_VMS_API LocalSiteAuth
{
    /**%apidoc New "admin" user password. */
    QString password;

    /**%apidoc User agent of the client. */
    std::optional<QString> userAgent;
};
#define LocalSystemAuth_Fields (password)(userAgent)
NX_VMS_API_DECLARE_STRUCT_EX(LocalSiteAuth, (json))
NX_REFLECTION_INSTRUMENT(LocalSiteAuth, LocalSystemAuth_Fields)

struct CloudAuthBase
{
    /**%apidoc The Site authentication key given by the Cloud. */
    QString authKey;

    /**%apidoc The cloud user that registered this Site on the Cloud. */
    QString owner;

    /**%apidoc User agent of the client. */
    std::optional<QString> userAgent;

    /**%apidoc[opt] The cloud user's organization. */
    QString organizationId;
};
#define CloudAuthBase_Fields (authKey)(owner)(userAgent)(organizationId)

struct NX_VMS_API CloudSystemAuth: CloudAuthBase
{
    /**%apidoc The Site Id given by the Cloud. */
    QString systemId;
};
#define CloudSystemAuth_Fields CloudAuthBase_Fields(systemId)
NX_VMS_API_DECLARE_STRUCT_EX(CloudSystemAuth, (json))
NX_REFLECTION_INSTRUMENT(CloudSystemAuth, CloudSystemAuth_Fields)

struct NX_VMS_API CloudSiteAuth: CloudAuthBase
{
    /**%apidoc The Site Id given by the Cloud. */
    QString siteId;
};
#define CloudSiteAuth_Fields CloudAuthBase_Fields(siteId)
NX_VMS_API_DECLARE_STRUCT_EX(CloudSiteAuth, (json))
NX_REFLECTION_INSTRUMENT(CloudSiteAuth, CloudSiteAuth_Fields)

NX_REFLECTION_ENUM_CLASS(SystemSettingsPreset,
    compatibility,
    recommended,
    security
)

struct SetupDataBase
{
    /**%apidoc New Site name.
     * %example Site 1
     */
    QString name;

    /**%apidoc[opt] Site settings preset. */
    SystemSettingsPreset settingsPreset = SystemSettingsPreset::recommended;

    /**%apidoc Site settings to set when Site is configured. */
    std::map<QString, QJsonValue> settings;

    /**%apidoc For local Site only. */
    std::optional<LocalSiteAuth> local;
};
#define SetupDataBase_Fields (name)(settingsPreset)(settings)(local)

struct NX_VMS_API SetupSystemData: SetupDataBase
{
    /**%apidoc For cloud Site only. */
    std::optional<CloudSystemAuth> cloud;
};
#define SetupSystemData_Fields SetupDataBase_Fields(cloud)
NX_VMS_API_DECLARE_STRUCT_EX(SetupSystemData, (json))

struct NX_VMS_API SetupSiteData: SetupDataBase
{
    /**%apidoc For cloud Site only. */
    std::optional<CloudSiteAuth> cloud;
};
#define SetupSiteData_Fields SetupDataBase_Fields(cloud)
NX_VMS_API_DECLARE_STRUCT_EX(SetupSiteData, (json))

} // namespace nx::vms::api
