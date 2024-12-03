// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>
#include <optional>

#include <nx/reflect/enum_instrument.h>

#include "system_settings.h"

namespace nx::vms::api {

struct NX_VMS_API LocalSiteAuth
{
    /**%apidoc New "admin" user password. */
    QString password;
};
#define LocalSystemAuth_Fields (password)
NX_VMS_API_DECLARE_STRUCT_EX(LocalSiteAuth, (json))
NX_REFLECTION_INSTRUMENT(LocalSiteAuth, LocalSystemAuth_Fields)

struct CloudAuthBase
{
    /**%apidoc The Site authentication key given by the Cloud. */
    QString authKey;

    /**%apidoc The cloud user that registered this Site on the Cloud. */
    QString owner;

    /**%apidoc[opt] The cloud user's organization. */
    QString organizationId;
};
#define CloudAuthBase_Fields (authKey)(owner)(organizationId)

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

    /**%apidoc For local Site only. */
    std::optional<LocalSiteAuth> local;
};
#define SetupDataBase_Fields (name)(settingsPreset)(local)

struct NX_VMS_API SetupSystemData: SetupDataBase
{
    /**%apidoc[opt] Site settings to set when Site is configured. */
    std::map<QString, QJsonValue> settings;

    /**%apidoc For cloud Site only. */
    std::optional<CloudSystemAuth> cloud;
};
#define SetupSystemData_Fields SetupDataBase_Fields(settings)(cloud)
NX_VMS_API_DECLARE_STRUCT_EX(SetupSystemData, (json))
NX_REFLECTION_INSTRUMENT(SetupSystemData, SetupSystemData_Fields)

/**%apidoc
 * %param[unused] settings.siteName
 */
struct NX_VMS_API SetupSiteData: SetupDataBase
{
    /**%apidoc[opt] Site settings to set when Site is configured. */
    SaveableSiteSettings settings;

    /**%apidoc For cloud Site only. */
    std::optional<CloudSiteAuth> cloud;
};
#define SetupSiteData_Fields SetupDataBase_Fields(settings)(cloud)
NX_VMS_API_DECLARE_STRUCT_EX(SetupSiteData, (json))
NX_REFLECTION_INSTRUMENT(SetupSiteData, SetupSiteData_Fields)

} // namespace nx::vms::api
