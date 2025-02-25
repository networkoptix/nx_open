// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/software_version.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/json/value_or_array.h>

namespace nx::vms::api {

struct ModuleInformation;

NX_REFLECTION_ENUM_CLASS(SiteCompatibilityStatus,
    compatible,
    incompatible,
    inaccessible
)

struct NX_VMS_API RestApiVersions
{
    std::string min;
    std::string max;

    static RestApiVersions current();
};
#define RestApiVersions_Fields (min)(max)
QN_FUSION_DECLARE_FUNCTIONS(RestApiVersions, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(RestApiVersions, RestApiVersions_Fields)

struct NX_VMS_API SiteInformation
{
    QString name;
    QString customization;
    nx::utils::SoftwareVersion version; /**<%apidoc:string */
    int protoVersion = 0;
    RestApiVersions restApiVersions;
    QString cloudHost;

    /**%apidoc If not present, the Site is new. */
    std::optional<nx::Uuid> localId;

    /**%apidoc If present, the Site is bound to the Cloud. */
    std::optional<QString> cloudId;

    /**%apidoc Presented if the Site is bound to the Cloud. */
    std::optional<nx::Uuid> cloudOwnerId;

    /**%apidoc Presented if the Site is bound to the Cloud and belongs to the Organization. */
    std::optional<nx::Uuid> organizationId;

    /**%apidoc For local Site only. */
    std::optional<std::vector<nx::Uuid>> servers;

    /**%apidoc Amount of edge Servers in the Site. */
    int edgeServerCount = 0;

    /**%apidoc For local Site only. */
    std::optional<std::vector<nx::Uuid>> devices;

    /**%apidoc For remote Sites only. */
    std::optional<QString> endpoint; //< TODO: SocketAddress?

    /**%apidoc For remote Sites only. */
    std::optional<SiteCompatibilityStatus> status;

    /**%apidoc[proprietary] Internal identification of LDAP connection. */
    std::optional<QString> ldapSyncId;

    /**%apidoc Synchronized time of the VMS Site, in milliseconds since epoch. */
    std::chrono::milliseconds synchronizedTimeMs{0};

    /**%apidoc Amount of pending Servers in the Site. */
    std::optional<int> pendingServerCount = 0;

    SiteInformation() = default;
    SiteInformation(const ModuleInformation& module);

    SiteInformation(const SiteInformation&) = default;
    SiteInformation(SiteInformation&&) = default;

    SiteInformation& operator=(const SiteInformation&) = default;
    SiteInformation& operator=(SiteInformation&&) = default;
};
#define SiteInformation_Fields \
    (name)(customization)(version)(protoVersion)(restApiVersions)(cloudHost)(localId)(cloudId) \
    (cloudOwnerId)(organizationId)(endpoint)(servers)(edgeServerCount)(devices)(status)(ldapSyncId) \
    (synchronizedTimeMs)(pendingServerCount)
QN_FUSION_DECLARE_FUNCTIONS(SiteInformation, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(SiteInformation, SiteInformation_Fields)

struct NX_VMS_API OtherSiteRequest
{
    /**%apidoc[opt] Whether discovered Sites should be in the list. */
    bool showDiscovered = true;

    /**%apidoc:stringArray Endpoint to ping and add into the result. */
    std::optional<nx::vms::api::json::ValueOrArray<QString>> endpoint;
};
#define OtherSiteRequest_Fields (showDiscovered)(endpoint)
QN_FUSION_DECLARE_FUNCTIONS(OtherSiteRequest, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(OtherSiteRequest, OtherSiteRequest_Fields)

} // namespace nx::vms::api
